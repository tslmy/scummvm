#!/usr/bin/env python3
"""Independently replay an opt-in Rose Tattoo native-mask diagnostic trace.

The trace contains game-derived data and should stay in a private local path.
Only the recorded scene boundary is verified; this does not verify later UI or
the hires compositor. No game data or third-party Python packages are needed.
"""

from __future__ import annotations

import argparse
import array
import struct
import sys
from pathlib import Path


MAGIC = b"RTMASK1\0"
FIELDS = struct.Struct("<II14i")
CALL = struct.Struct("<IiiiiI")


class TraceError(ValueError):
    pass


class Reader:
    def __init__(self, data: bytes):
        self.data = memoryview(data)
        self.pos = 0

    def read(self, size: int) -> memoryview:
        if size < 0 or size > len(self.data) - self.pos:
            raise TraceError("truncated record")
        result = self.data[self.pos : self.pos + size]
        self.pos += size
        return result

    def unpack(self, layout: struct.Struct) -> tuple[int, ...]:
        return layout.unpack(self.read(layout.size))


def expected_calls(scene: int, mode: int, offset: int, viewport: int,
                   flag: int, value: int) -> list[tuple[int, int, int]]:
    if mode not in (0, 1):
        raise TraceError("invalid mode")
    if scene in (8, 18):
        expected_flag = 880 if scene == 8 else 189
        if flag != expected_flag or value not in (0, 1):
            raise TraceError("invalid recorded conditional")
    elif (flag, value) != (-1, -1):
        raise TraceError("unexpected recorded conditional")

    if scene in (7, 8):
        y = 110 if scene == 7 else 180
        calls = [(1, offset - viewport, y), (1, offset, y), (1, offset + viewport, y)]
        if scene == 8 and value == 0:
            calls.append((2, 940, 300))
        return calls
    if scene in (18, 68):
        xp = offset if mode else 0
        calls = [(1, xp, 203)]
        if scene == 68 or value == 0:
            calls.append((2, 124 + xp, 239))
        return calls
    if scene == 53:
        calls = [(1, offset, 110)]
        if mode:
            calls.append((1, offset - viewport, 110))
        return calls
    raise TraceError(f"unexpected mask scene {scene}")


def verify_record(data: bytes) -> dict[str, int]:
    reader = Reader(data)
    (serial, epoch, scene, mode, width, height, scroll_x, scroll_y,
     offset_x, offset_y, viewport, flag, value, call_count, out_of_bounds,
     reported_mismatches) = reader.unpack(FIELDS)
    if not (0 < width <= 8192 and 0 < height <= 8192 and width * height <= 16_777_216):
        raise TraceError("invalid world dimensions")
    if not (0 < viewport <= 8192 and 0 <= call_count <= 16):
        raise TraceError("invalid viewport or call count")
    lookup = reader.read(256)
    reader.read(768)  # Installed display palette at the mask boundary.
    n = width * height
    before = reader.read(n)
    after = reader.read(n)
    stored_hits = reader.read(n * 2)
    reconstructed = array.array("H", [0]) * n
    actual_calls: list[tuple[int, int, int]] = []
    hits_236 = 0
    # Multiple placements of one native stream must read the same RLE data.
    sources: dict[int, tuple[int, int, bytes]] = {}

    for _ in range(call_count):
        source, x, y, call_width, call_height, run_size = reader.unpack(CALL)
        if not (0 < call_width <= 8192 and 0 < call_height <= 8192):
            raise TraceError("invalid mask dimensions")
        if run_size % 2:
            raise TraceError("odd RLE byte count")
        runs = reader.read(run_size)
        resource = (call_width, call_height, runs.tobytes())
        if source in sources and sources[source] != resource:
            raise TraceError("repeated calls to a mask source used different data")
        sources[source] = resource
        actual_calls.append((source, x, y))
        run_pos = 0
        for row in range(call_height):
            xp = 0
            while xp < call_width:
                if run_pos + 2 > len(runs):
                    raise TraceError("truncated RLE row")
                pixel, length = runs[run_pos], runs[run_pos + 1]
                run_pos += 2
                if not length or xp + length > call_width:
                    raise TraceError("invalid RLE run length")
                if pixel:
                    left = max(x + xp, scroll_x, 0)
                    right = min(x + xp + length, scroll_x + viewport, width)
                    if left < right:
                        if not 0 <= y + row < height:
                            raise TraceError("native hit lies outside the recorded buffer")
                        if pixel == 236:
                            hits_236 += right - left
                        base = (y + row) * width
                        for column in range(left, right):
                            idx = base + column
                            if reconstructed[idx] == 65535:
                                raise TraceError("hit count overflow")
                            reconstructed[idx] += 1
                xp += length
        if run_pos != len(runs):
            raise TraceError("extra RLE bytes")

    if reader.pos != len(reader.data):
        raise TraceError("extra bytes after record")
    if actual_calls != expected_calls(scene, mode, offset_x, viewport, flag, value):
        raise TraceError("mask call order, position, mode or conditional differs from native rules")
    if out_of_bounds or reported_mismatches:
        raise TraceError("engine reported an invalid hit or indexed replay mismatch")

    # Compare independently decoded run coverage to the instrumented hit plane.
    encoded_hits = array.array("H", reconstructed)
    if sys.byteorder != "little":
        encoded_hits.byteswap()
    if encoded_hits.tobytes() != stored_hits.tobytes():
        raise TraceError("RLE-reconstructed hits differ from recorded native hits")

    # Then compare the complete indexed buffer, not only changed pixels or
    # nonzero hits. Overlaps apply the original lookup repeatedly.
    replayed = bytearray(before)
    for i, count in enumerate(reconstructed):
        for _ in range(count):
            replayed[i] = lookup[replayed[i]]
    if replayed != after.tobytes():
        raise TraceError("RLE replay differs byte-for-byte from native post-mask pixels")

    return {"serial": serial, "epoch": epoch, "scene": scene, "mode": mode,
            "scroll_x": scroll_x, "scroll_y": scroll_y, "offset_x": offset_x,
            "offset_y": offset_y, "flag": flag, "flag_value": value,
            "calls": call_count, "hits": sum(reconstructed),
            "hits_236": hits_236,
            "overlap_pixels": sum(count > 1 for count in reconstructed),
            "unchanged_hit_pixels": sum(count > 0 and a == b for count, a, b in zip(reconstructed, before, after)),
            "changed_pixels": sum(a != b for a, b in zip(before, after))}


def verify_file(path: Path) -> list[dict[str, int]]:
    reader = Reader(path.read_bytes())
    if reader.read(len(MAGIC)).tobytes() != MAGIC:
        raise TraceError("bad trace header")
    records = []
    while reader.pos < len(reader.data):
        (size,) = reader.unpack(struct.Struct("<I"))
        record = verify_record(reader.read(size).tobytes())
        if records and (record["serial"] <= records[-1]["serial"] or
                        record["epoch"] < records[-1]["epoch"]):
            raise TraceError("non-monotonic sample or scene epoch")
        records.append(record)
    if not records:
        raise TraceError("trace contains no records")
    return records


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("trace", nargs="+", type=Path)
    parser.add_argument("--require-scenes", nargs="*", type=int, default=[])
    parser.add_argument("--require-both-modes", action="store_true")
    args = parser.parse_args()
    all_records = []
    try:
        for path in args.trace:
            records = verify_file(path)
            all_records.extend(records)
            for record in records:
                print(f"{path}: " + " ".join(f"{key}={value}" for key, value in record.items()))
        for scene in args.require_scenes:
            modes = {record["mode"] for record in all_records if record["scene"] == scene}
            if not modes or (args.require_both_modes and modes != {0, 1}):
                raise TraceError(f"scene {scene} lacks required sample(s): modes={sorted(modes)}")
    except (OSError, TraceError) as error:
        print(f"mask trace verification failed: {error}", file=sys.stderr)
        return 1
    print(f"verified {len(all_records)} native mask boundary record(s)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

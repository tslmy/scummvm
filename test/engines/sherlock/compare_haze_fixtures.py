#!/usr/bin/env python3
"""Compare controlled native-reference and scene-fallback mask traces.

Both files must come from the same isolated game state and deterministic
capture point. The first sample for each scene/mode/flag state is compared;
this intentionally fails if scene timing, scroll or native pixels differ.
It does not compare the later UI, cursor, or presented RGB frame.
"""

from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

import verify_mask_trace as trace


def first_samples(path: Path) -> dict[tuple[int, int, int, int], bytes]:
    reader = trace.Reader(path.read_bytes())
    if reader.read(len(trace.MAGIC)).tobytes() != trace.MAGIC:
        raise trace.TraceError("bad trace header")
    samples = {}
    previous_serial, previous_epoch = -1, -1
    while reader.pos < len(reader.data):
        (size,) = reader.unpack(struct.Struct("<I"))
        payload = reader.read(size).tobytes()
        metadata = trace.verify_record(payload)
        if metadata["serial"] <= previous_serial or metadata["epoch"] < previous_epoch:
            raise trace.TraceError("non-monotonic sample or scene epoch")
        previous_serial, previous_epoch = metadata["serial"], metadata["epoch"]
        key = (metadata["scene"], metadata["mode"], metadata["flag"], metadata["flag_value"])
        # Serial and epoch are capture bookkeeping, not visual/native state.
        samples.setdefault(key, payload[8:])
    if not samples:
        raise trace.TraceError("trace contains no records")
    return samples


def compare(reference: Path, candidate: Path, required: list[int], both_modes: bool) -> int:
    a, b = first_samples(reference), first_samples(candidate)
    if a.keys() != b.keys():
        raise trace.TraceError(f"different sampled scene/mode/flag states: {sorted(a)} vs {sorted(b)}")
    for key in a:
        if a[key] != b[key]:
            raise trace.TraceError(f"indexed mask boundary differs for {key}")
    for scene in required:
        modes = {key[1] for key in a if key[0] == scene}
        if not modes or (both_modes and modes != {0, 1}):
            raise trace.TraceError(f"scene {scene} lacks required sample(s): modes={sorted(modes)}")
    return len(a)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference", type=Path)
    parser.add_argument("candidate", type=Path)
    parser.add_argument("--require-scenes", nargs="*", type=int, default=[])
    parser.add_argument("--require-both-modes", action="store_true")
    args = parser.parse_args()
    try:
        count = compare(args.reference, args.candidate, args.require_scenes, args.require_both_modes)
    except (OSError, trace.TraceError) as error:
        print(f"haze fixture comparison failed: {error}", file=sys.stderr)
        return 1
    print(f"matched {count} independently verified indexed mask-boundary fixture(s)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

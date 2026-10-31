import array
import struct
import sys
import unittest

import verify_mask_trace as trace


def sample(mode=1, identity=False):
    width, height, y = 6, 111, 110
    n = width * height
    lookup = bytearray(range(256))
    if not identity:
        lookup[5], lookup[6] = 6, 7
    before = bytes([5]) * n
    after = bytearray(before)
    hits = array.array("H", [0]) * n
    for x in range(2, 6):
        hits[y * width + x] = 2 if mode else 1
        after[y * width + x] = 5 if identity else (7 if mode else 6)
    if sys.byteorder != "little":
        hits.byteswap()
    # One 646-pixel mask is drawn at x=0 and (in mode 1) x=-640.
    # It has active value 236 at both ends, with long zero RLE runs between.
    runs = bytes([236, 6, 0, 255, 0, 255, 0, 124, 236, 6])
    calls = [(1, 0, y, 646, 1, len(runs))]
    if mode:
        calls.append((1, -640, y, 646, 1, len(runs)))
    fields = (1, 2, 53, mode, width, height, 2, 0, 0, 0, 640,
              -1, -1, len(calls), 0, 0)
    record = bytearray(trace.FIELDS.pack(*fields))
    record.extend(lookup)
    record.extend(bytes(768))
    record.extend(before)
    record.extend(after)
    record.extend(hits.tobytes())
    for call in calls:
        record.extend(trace.CALL.pack(*call))
        record.extend(runs)
    return record


class MaskTraceVerifierTest(unittest.TestCase):
    def test_overlap_nonzero_236_and_scrolled_clip(self):
        result = trace.verify_record(sample())
        self.assertEqual(result["hits"], 8)
        self.assertEqual(result["hits_236"], 8)
        self.assertEqual(result["overlap_pixels"], 4)
        self.assertEqual(result["changed_pixels"], 4)
        self.assertEqual(result["scroll_x"], 2)

    def test_false_mode_omits_second_wrapped_call(self):
        result = trace.verify_record(sample(mode=0))
        self.assertEqual(result["calls"], 1)
        self.assertEqual(result["hits"], 4)
        self.assertEqual(result["overlap_pixels"], 0)

    def test_identity_hits_are_still_verified(self):
        result = trace.verify_record(sample(identity=True))
        self.assertEqual(result["hits"], 8)
        self.assertEqual(result["changed_pixels"], 0)
        self.assertEqual(result["unchanged_hit_pixels"], 4)

    def test_corrupt_hits_and_post_pixels_are_rejected(self):
        data = sample()
        n = 6 * 111
        prefix = trace.FIELDS.size + 256 + 768
        data[prefix + 2 * n + 2 * (110 * 6 + 2)] = 0
        with self.assertRaisesRegex(trace.TraceError, "hits differ"):
            trace.verify_record(data)
        data = sample()
        data[prefix + n + 110 * 6 + 2] ^= 1
        with self.assertRaisesRegex(trace.TraceError, "byte-for-byte"):
            trace.verify_record(data)

    def test_inconsistent_repeated_source_and_bad_rle_are_rejected(self):
        data = sample()
        data[-10] = 255
        with self.assertRaisesRegex(trace.TraceError, "different data"):
            trace.verify_record(data)
        data = sample(mode=0)
        data[-9] = 0
        with self.assertRaisesRegex(trace.TraceError, "invalid RLE"):
            trace.verify_record(data)

    def test_flag_and_false_mode_placement_rules(self):
        self.assertEqual(trace.expected_calls(18, 0, 3, 640, 189, 0),
                         [(1, 0, 203), (2, 124, 239)])
        self.assertEqual(trace.expected_calls(18, 1, -2, 640, 189, 1),
                         [(1, -2, 203)])
        self.assertEqual(trace.expected_calls(68, 0, 3, 640, -1, -1),
                         [(1, 0, 203), (2, 124, 239)])
        self.assertEqual(trace.expected_calls(8, 1, 3, 640, 880, 0)[-1], (2, 940, 300))
        self.assertEqual(len(trace.expected_calls(8, 1, 3, 640, 880, 1)), 3)
        with self.assertRaises(trace.TraceError):
            trace.expected_calls(18, 0, 3, 640, 880, 0)

    def test_scene_epochs_and_truncated_files(self):
        first = sample(mode=0)
        second = sample(mode=1)
        struct.pack_into("<II", second, 0, 2, 3)

        def file_bytes(records):
            return trace.MAGIC + b"".join(struct.pack("<I", len(r)) + r for r in records)

        class MemoryPath:
            def __init__(self, data):
                self.data = data

            def read_bytes(self):
                return self.data

        data = file_bytes([first, second])
        records = trace.verify_file(MemoryPath(data))
        self.assertEqual([(r["serial"], r["epoch"]) for r in records], [(1, 2), (2, 3)])
        with self.assertRaisesRegex(trace.TraceError, "truncated"):
            trace.verify_file(MemoryPath(data[:-1]))
        struct.pack_into("<II", second, 0, 2, 1)
        with self.assertRaisesRegex(trace.TraceError, "epoch"):
            trace.verify_file(MemoryPath(file_bytes([first, second])))


if __name__ == "__main__":
    unittest.main()

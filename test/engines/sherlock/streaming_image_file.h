#include <cxxtest/TestSuite.h>

#include "common/array.h"
#include "common/memstream.h"
#include "sherlock/image_file.h"

class SherlockTrackedReadStream : public Common::MemoryReadStream {
public:
	SherlockTrackedReadStream(const byte *data, uint32 size, bool &destroyed)
		: Common::MemoryReadStream(data, size, DisposeAfterUse::NO), _destroyed(destroyed) {}
	~SherlockTrackedReadStream() override { _destroyed = true; }
private:
	bool &_destroyed;
};

class SherlockStreamingImageFileTestSuite : public CxxTest::TestSuite {
public:
	void testUncompressedFrameUsesItsOwnPayload() {
		// A compressed literal frame seeds the reusable LZ buffer with pixel 61.
		// Reuse the same decoder for the original one-pixel uncompressed
		// regression fixture, whose pixel is 123, so stale-buffer reads are
		// deterministic and cannot accidentally pass on uninitialized memory.
		const byte compressed[] = {
			14, 0, 0, 0,
			0xff, 0, 0, 0, 0, 0, 0, 1, 2,
			0xff, 12, 0, 254, 61
		};
		const byte uncompressed[] = { 0, 0, 0, 0, 0, 0, 0, 0, 12, 0, 0, 123 };
		bool compressedDestroyed = false, uncompressedDestroyed = false;
		Sherlock::StreamingImageFile images;
		images.load(new SherlockTrackedReadStream(compressed, sizeof(compressed), compressedDestroyed), true);
		const bool compressedOk = images.getNextFrame();
		TS_ASSERT(compressedOk);
		if (!compressedOk)
			return;
		TS_ASSERT_EQUALS(images._imageFrame._frame.getPixel(0, 0), 61);
		TS_ASSERT(!compressedDestroyed);
		images.close();
		TS_ASSERT(compressedDestroyed);

		images.load(new SherlockTrackedReadStream(uncompressed, sizeof(uncompressed), uncompressedDestroyed), false);
		TS_ASSERT_EQUALS(images.frameNumber(), -1);
		const bool uncompressedOk = images.getNextFrame();
		TS_ASSERT(uncompressedOk);
		if (!uncompressedOk)
			return;
		TS_ASSERT_EQUALS(images.frameNumber(), 0);
		TS_ASSERT_EQUALS(images._imageFrame._width, 1);
		TS_ASSERT_EQUALS(images._imageFrame._height, 1);
		TS_ASSERT_EQUALS(images._imageFrame._offset.x, 0);
		TS_ASSERT_EQUALS(images._imageFrame._offset.y, 0);
		TS_ASSERT_EQUALS(images._imageFrame._frame.getPixel(0, 0), 123);
		TS_ASSERT(!uncompressedDestroyed);
		TS_ASSERT(!images.getNextFrame());
		TS_ASSERT(!images.active());
		TS_ASSERT(!uncompressedDestroyed);
		images.close();
		TS_ASSERT(uncompressedDestroyed);
	}

	void testUncompressedFrameSequence() { checkFrameSequence(false); }
	void testCompressedFrameSequence() { checkFrameSequence(true); }

	void testOtherImageFrameEncodings() {
		// ImageFrame decoding is also shared by Serrated Scalpel and by
		// non-streamed nibble-packed images.
		const byte scalpelRle[] = { 5, 254, 13, 3, 7, 9 };
		const byte scalpelPixels[] = { 5, 13, 13, 13, 7, 9 };
		Sherlock::ImageFrame frame = {};
		frame._width = 3;
		frame._height = 2;
		frame._size = sizeof(scalpelRle);
		frame._rleEncoded = true;
		frame._rleMarker = 254;
		frame.decompressFrame(scalpelRle, false);
		checkFrame(frame, 3, 2, 0, 0, scalpelPixels);
		frame._frame.free();

		const byte nibble[] = { 0x21, 0xf0 };
		const byte nibblePixels[] = { 1, 2, 0, 15 };
		frame._width = 4;
		frame._height = 1;
		frame._size = sizeof(nibble);
		frame._paletteBase = 1;
		frame._rleEncoded = false;
		frame.decompressFrame(nibble, true);
		checkFrame(frame, 4, 1, 0, 0, nibblePixels);
		frame._frame.free();
	}

	void testCompressedBackReference() {
		// Four literal header/payload bytes followed by a three-byte LZ copy
		// from the first payload byte in the sliding window.
		const byte compressed[] = {
			16, 0, 0, 0,
			0xff, 3, 0, 0, 0, 0, 0, 1, 2,
			0x0f, 15, 0, 0, 7, 0xf9, 0xf0
		};
		bool destroyed = false;
		{
			Sherlock::StreamingImageFile images;
			images.load(new SherlockTrackedReadStream(compressed, sizeof(compressed), destroyed), true);
			const bool ok = images.getNextFrame();
			TS_ASSERT(ok);
			if (!ok)
				return;
			TS_ASSERT_EQUALS(images._imageFrame._width, 4);
			TS_ASSERT_EQUALS(images._imageFrame._height, 1);
			TS_ASSERT_EQUALS(images._imageFrame._offset.x, 1);
			TS_ASSERT_EQUALS(images._imageFrame._offset.y, 2);
			for (int x = 0; x < 4; ++x)
				TS_ASSERT_EQUALS(images._imageFrame._frame.getPixel(x, 0), 7);
			TS_ASSERT(!destroyed);
		}
		// Destruction, not just close(), owns and deletes the input stream.
		TS_ASSERT(destroyed);
	}

	void testLargeUncompressedFrame() {
		// 4 * 16381 = 65524 bytes, the largest payload representable by
		// the 16-bit frame size after subtracting the 11-byte header.
		const uint width = 4, height = 16381, payloadSize = width * height;
		const uint frameSize = payloadSize + 11;
		Common::Array<byte> bytes;
		bytes.resize(frameSize, 0);
		bytes[0] = (width - 1) & 0xff;
		bytes[1] = (width - 1) >> 8;
		bytes[2] = (height - 1) & 0xff;
		bytes[3] = (height - 1) >> 8;
		bytes[6] = 255;
		bytes[7] = 254;
		bytes[8] = frameSize & 0xff;
		bytes[9] = frameSize >> 8;
		for (uint i = 0; i < payloadSize; ++i)
			bytes[11 + i] = (byte)(i * 17 + 3);

		bool destroyed = false;
		Sherlock::StreamingImageFile images;
		images.load(new SherlockTrackedReadStream(&bytes[0], bytes.size(), destroyed), false);
		const bool ok = images.getNextFrame();
		TS_ASSERT(ok);
		if (!ok)
			return;
		TS_ASSERT_EQUALS(images._imageFrame._width, width);
		TS_ASSERT_EQUALS(images._imageFrame._height, height);
		TS_ASSERT_EQUALS(images._imageFrame._offset.x, 255);
		TS_ASSERT_EQUALS(images._imageFrame._offset.y, 254);
		TS_ASSERT_EQUALS(images._imageFrame._frame.pitch, width);
		if (images._imageFrame._frame.w == (int)width && images._imageFrame._frame.h == (int)height &&
				images._imageFrame._frame.pitch == width)
			TS_ASSERT_SAME_DATA(images._imageFrame._frame.getPixels(), &bytes[11], payloadSize);
		TS_ASSERT(!images.getNextFrame());
		TS_ASSERT(!destroyed);
		images.close();
		TS_ASSERT(destroyed);
	}

	void testUncompressedTruncatedAndInvalidSizes() {
		// Reject underflow, short reads and payloads that would read or write
		// beyond their declared dimensions or encoded data.
		const byte invalidSize[] = { 0, 0, 0, 0, 0, 0, 0, 0, 10, 0, 0 };
		const byte truncated[] = { 1, 0, 0, 0, 0, 0, 0, 0, 13, 0, 0, 42 };
		const byte shortRaw[] = { 1, 0, 0, 0, 0, 0, 0, 0, 12, 0, 0, 42 };
		const byte oversizedNibble[] = { 0, 0, 0, 0, 1, 0, 0, 0, 12, 0, 0, 0x21 };
		const byte invalidDimension[] = { 255, 255, 0, 0, 0, 0, 0, 0, 12, 0, 0, 42 };
		const byte wideDimension[] = { 255, 127, 0, 0, 0, 1, 0, 0, 12, 0, 0, 255 };
		const byte excessiveSkip[] = { 1, 0, 0, 0, 0, 1, 0, 0, 12, 0, 0, 3 };
		const byte excessiveRun[] = { 1, 0, 0, 0, 0, 1, 0, 0, 16, 0, 0, 0, 3, 7, 8, 9 };
		const byte shortRun[] = { 1, 0, 0, 0, 0, 1, 0, 0, 14, 0, 0, 0, 2, 7 };
		const byte stalledRun[] = { 0, 0, 0, 0, 0, 1, 0, 0, 13, 0, 0, 0, 0 };
		checkInvalidStream(invalidSize, sizeof(invalidSize));
		checkInvalidStream(truncated, sizeof(truncated));
		checkInvalidStream(shortRaw, sizeof(shortRaw));
		checkInvalidStream(oversizedNibble, sizeof(oversizedNibble));
		checkInvalidStream(invalidDimension, sizeof(invalidDimension));
		checkInvalidStream(wideDimension, sizeof(wideDimension));
		checkInvalidStream(excessiveSkip, sizeof(excessiveSkip));
		checkInvalidStream(excessiveRun, sizeof(excessiveRun));
		checkInvalidStream(shortRun, sizeof(shortRun));
		checkInvalidStream(stalledRun, sizeof(stalledRun));
	}

private:
	static void appendLiteralFrame(Common::Array<byte> &output, const byte *frame, uint32 size) {
		// A Rose Tattoo compressed chunk has a little-endian compressed size
		// followed by LZ groups; 0xff marks each group of up to eight literals.
		const uint32 encodedSize = size + (size + 7) / 8;
		for (int shift = 0; shift < 32; shift += 8)
			output.push_back((byte)(encodedSize >> shift));
		for (uint32 i = 0; i < size;) {
			output.push_back(0xff);
			for (int n = 0; n < 8 && i < size; ++n)
				output.push_back(frame[i++]);
		}
	}

	static void checkFrameSequence(bool compressed) {
		// Three-by-two raw frame, followed by a four-by-two Rose Tattoo RLE
		// frame. The final RLE skip is exactly the last byte of the payload.
		const byte raw[] = {
			2, 0, 1, 0, 0, 0, 7, 11, 17, 0, 254,
			2, 0, 255, 42, 123, 17
		};
		const byte rle[] = {
			3, 0, 1, 0, 0, 1, 3, 5, 22, 0, 254,
			1, 2, 4, 0, 1, 0, 3, 12, 34, 56, 1
		};
		const byte nibble[] = { 3, 0, 0, 0, 1, 0, 9, 10, 13, 0, 0, 0x21, 0xf0 };
		// A zero-length run is valid when the preceding skip made progress.
		const byte skipOnly[] = { 2, 0, 0, 0, 0, 1, 2, 4, 17, 0, 0, 1, 0, 0, 2, 6, 7 };
		// A no-op skip/run pair also consumes input and may precede a run.
		const byte noOp[] = { 0, 0, 0, 0, 0, 1, 1, 6, 16, 0, 0, 0, 0, 0, 1, 42 };
		const byte rawPixels[] = { 2, 0, 255, 42, 123, 17 };
		const byte rlePixels[] = { 255, 4, 0, 255, 12, 34, 56, 255 };
		const byte nibblePixels[] = { 1, 2, 0, 15 };
		const byte skipOnlyPixels[] = { 255, 6, 7 };
		const byte noOpPixels[] = { 42 };
		Common::Array<byte> bytes;
		if (compressed) {
			appendLiteralFrame(bytes, raw, sizeof(raw));
			appendLiteralFrame(bytes, rle, sizeof(rle));
			appendLiteralFrame(bytes, nibble, sizeof(nibble));
			appendLiteralFrame(bytes, skipOnly, sizeof(skipOnly));
			appendLiteralFrame(bytes, noOp, sizeof(noOp));
		} else {
			for (uint i = 0; i < sizeof(raw); ++i)
				bytes.push_back(raw[i]);
			for (uint i = 0; i < sizeof(rle); ++i)
				bytes.push_back(rle[i]);
			for (uint i = 0; i < sizeof(nibble); ++i)
				bytes.push_back(nibble[i]);
			for (uint i = 0; i < sizeof(skipOnly); ++i)
				bytes.push_back(skipOnly[i]);
			for (uint i = 0; i < sizeof(noOp); ++i)
				bytes.push_back(noOp[i]);
		}

		bool destroyed = false;
		Sherlock::StreamingImageFile images;
		images.load(new SherlockTrackedReadStream(&bytes[0], bytes.size(), destroyed), compressed);
		TS_ASSERT(images.active());
		bool ok = images.getNextFrame();
		TS_ASSERT(ok);
		if (!ok)
			return;
		TS_ASSERT_EQUALS(images.frameNumber(), 0);
		checkFrame(images._imageFrame, 3, 2, 7, 11, rawPixels);
		ok = images.getNextFrame();
		TS_ASSERT(ok);
		if (!ok)
			return;
		TS_ASSERT_EQUALS(images.frameNumber(), 1);
		checkFrame(images._imageFrame, 4, 2, 3, 5, rlePixels);
		ok = images.getNextFrame();
		TS_ASSERT(ok);
		if (!ok)
			return;
		TS_ASSERT_EQUALS(images.frameNumber(), 2);
		checkFrame(images._imageFrame, 4, 1, 9, 10, nibblePixels);
		ok = images.getNextFrame();
		TS_ASSERT(ok);
		if (!ok)
			return;
		TS_ASSERT_EQUALS(images.frameNumber(), 3);
		checkFrame(images._imageFrame, 3, 1, 2, 4, skipOnlyPixels);
		ok = images.getNextFrame();
		TS_ASSERT(ok);
		if (!ok)
			return;
		TS_ASSERT_EQUALS(images.frameNumber(), 4);
		checkFrame(images._imageFrame, 1, 1, 1, 6, noOpPixels);
		TS_ASSERT(!destroyed);
		TS_ASSERT(!images.getNextFrame());
		TS_ASSERT(!images.active());
		TS_ASSERT(!destroyed);
		images.close();
		TS_ASSERT(destroyed);
		TS_ASSERT_EQUALS(images.frameNumber(), -1);
	}

	static void checkFrame(const Sherlock::ImageFrame &frame, int width, int height, int xOffset, int yOffset,
			const byte *pixels) {
		TS_ASSERT_EQUALS(frame._width, width);
		TS_ASSERT_EQUALS(frame._height, height);
		TS_ASSERT_EQUALS(frame._offset.x, xOffset);
		TS_ASSERT_EQUALS(frame._offset.y, yOffset);
		TS_ASSERT_EQUALS(frame._frame.w, width);
		TS_ASSERT_EQUALS(frame._frame.h, height);
		if (frame._frame.w == width && frame._frame.h == height) {
			for (int y = 0; y < height; ++y)
				for (int x = 0; x < width; ++x)
					TS_ASSERT_EQUALS(frame._frame.getPixel(x, y), pixels[y * width + x]);
		}
	}

	static void checkInvalidStream(const byte *bytes, uint32 size) {
		bool destroyed = false;
		Sherlock::StreamingImageFile images;
		images.load(new SherlockTrackedReadStream(bytes, size, destroyed), false);
		TS_ASSERT(!images.getNextFrame());
		TS_ASSERT(!images.active());
		TS_ASSERT(!destroyed);
		TS_ASSERT(!images._imageFrame._frame.getPixels());
		images.close();
		TS_ASSERT(destroyed);
	}
};

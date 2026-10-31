#include <cxxtest/TestSuite.h>

#include "common/memstream.h"
#include "sherlock/tattoo/mask_trace.h"

class SherlockTattooMaskTraceTestSuite : public CxxTest::TestSuite {
private:
	void initTables(byte *lookup, byte *palette) {
		for (uint i = 0; i < Graphics::PALETTE_COUNT; ++i)
			lookup[i] = (byte)(i + 1);
		Common::fill(palette, palette + Graphics::PALETTE_SIZE, 0);
	}

public:
	void testActualHitRecordingKeepsOverlapIdentityAndLateUiSeparate() {
		byte lookup[Graphics::PALETTE_COUNT], palette[Graphics::PALETTE_SIZE];
		initTables(lookup, palette);
		lookup[42] = 42; // A real hit can leave its indexed pixel unchanged.
		Graphics::ManagedSurface parent(8, 4, Graphics::PixelFormat::createFormatCLUT8());
		parent.clear(10);
		Graphics::ManagedSurface world(parent, Common::Rect(1, 1, 7, 4));
		world.setPixel(2, 1, 42);
		Sherlock::Tattoo::MaskTraceFrame frame(4, 2, 53, true, world,
			Common::Point(2, 0), Common::Point(1, 0), 3, lookup, palette);

		frame.beginCall(1, Common::Point(-2, 1), 6, 1);
		frame.recordRun(0, 1);
		frame.recordRun(236, 4);
		frame.recordRun(0, 1);
		// These are the hits the native horizontal clip admitted.
		frame.recordHit(2, 1);
		world.setPixel(2, 1, lookup[world.getPixel(2, 1)]);
		frame.beginCall(2, Common::Point(2, 1), 2, 1);
		frame.recordRun(255, 2);
		for (int x = 2; x < 4; ++x) {
			frame.recordHit(x, 1);
			world.setPixel(x, 1, lookup[world.getPixel(x, 1)]);
		}
		TS_ASSERT(frame.finish(world));
		TS_ASSERT_EQUALS(frame.mismatches, 0);
		TS_ASSERT_EQUALS(frame.outOfBoundsHits, 0);
		TS_ASSERT_EQUALS(frame.hits[1 * 6 + 2], 2);
		TS_ASSERT_EQUALS(frame.before[1 * 6 + 2], 42);
		TS_ASSERT_EQUALS(frame.after[1 * 6 + 2], 42);
		TS_ASSERT_EQUALS(frame.hits[1 * 6 + 3], 1);
		TS_ASSERT_EQUALS(frame.after[1 * 6 + 3], 11);
		TS_ASSERT_EQUALS(frame.hits[1 * 6 + 1], 0);
		TS_ASSERT_EQUALS(frame.calls.size(), 2);
		TS_ASSERT_EQUALS(frame.calls[0].runs[2], 236);
		TS_ASSERT_EQUALS(frame.calls[1].source, 2);

		// A later UI write is not retroactively included in the scene boundary.
		world.setPixel(3, 1, 99);
		TS_ASSERT_EQUALS(frame.after[1 * 6 + 3], 11);
		Sherlock::Tattoo::MaskTraceFrame next(5, 3, 18, false, world,
			Common::Point(1, 0), Common::Point(3, 0), 3, lookup, palette);
		TS_ASSERT_EQUALS(next.epoch, 3);
		TS_ASSERT_EQUALS(next.mode, 0);
		TS_ASSERT_EQUALS(next.before[1 * 6 + 3], 99);
		TS_ASSERT_EQUALS(next.hits[1 * 6 + 3], 0);
	}

	void testDetectsMissingHitAndInvalidCoordinate() {
		byte lookup[Graphics::PALETTE_COUNT], palette[Graphics::PALETTE_SIZE];
		initTables(lookup, palette);
		Graphics::ManagedSurface world(2, 1, Graphics::PixelFormat::createFormatCLUT8());
		world.clear(5);
		Sherlock::Tattoo::MaskTraceFrame frame(0, 0, 7, false, world,
			Common::Point(0, 0), Common::Point(0, 0), 2, lookup, palette);
		frame.recordHit(-1, 0);
		world.setPixel(1, 0, 6); // Native changed but the hit was not recorded.
		TS_ASSERT(frame.finish(world));
		TS_ASSERT_EQUALS(frame.outOfBoundsHits, 1);
		TS_ASSERT_EQUALS(frame.mismatches, 1);
	}

	void testSerializedMetadataIncludesFalseModeAndFlag() {
		byte lookup[Graphics::PALETTE_COUNT], palette[Graphics::PALETTE_SIZE];
		initTables(lookup, palette);
		Graphics::ManagedSurface world(2, 1, Graphics::PixelFormat::createFormatCLUT8());
		world.clear(5);
		Sherlock::Tattoo::MaskTraceFrame frame(8, 3, 18, false, world,
			Common::Point(1, 0), Common::Point(-2, 0), 640, lookup, palette);
		frame.recordCondition(189, true);
		TS_ASSERT(frame.finish(world));
		Common::MemoryWriteStreamDynamic payload(DisposeAfterUse::YES);
		frame.write(payload);
		Common::MemoryReadStream input(payload.getData(), payload.size());
		TS_ASSERT_EQUALS(input.readUint32LE(), 8);
		TS_ASSERT_EQUALS(input.readUint32LE(), 3);
		TS_ASSERT_EQUALS(input.readUint32LE(), 18);
		TS_ASSERT_EQUALS(input.readUint32LE(), 0);
		TS_ASSERT_EQUALS(input.readUint32LE(), 2);
		TS_ASSERT_EQUALS(input.readUint32LE(), 1);
		TS_ASSERT_EQUALS(input.readUint32LE(), 1);
		TS_ASSERT_EQUALS(input.readUint32LE(), 0);
		TS_ASSERT_EQUALS((int32)input.readUint32LE(), -2);
		TS_ASSERT_EQUALS(input.readUint32LE(), 0);
		TS_ASSERT_EQUALS(input.readUint32LE(), 640);
		TS_ASSERT_EQUALS(input.readUint32LE(), 189);
		TS_ASSERT_EQUALS(input.readUint32LE(), 1);
		TS_ASSERT_EQUALS(input.readUint32LE(), 0); // call count
	}
};

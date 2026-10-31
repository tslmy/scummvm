#include <cxxtest/TestSuite.h>

#include "sherlock/hires_palette.h"

class SherlockHiresPaletteTestSuite : public CxxTest::TestSuite {
public:
	void testInitialPaletteAndPartialUpdates() {
		Sherlock::RoseTattooHiresPalette active;
		byte result[Graphics::PALETTE_SIZE];
		active.get(result, 0, Graphics::PALETTE_COUNT);
		for (uint i = 0; i < Graphics::PALETTE_SIZE; ++i)
			TS_ASSERT_EQUALS(result[i], 0);

		byte room[Graphics::PALETTE_SIZE];
		for (uint i = 0; i < Graphics::PALETTE_SIZE; ++i)
			room[i] = i % 256;
		active.set(room, 0, Graphics::PALETTE_COUNT);

		// Partial input starts at its first entry, not at start * 3. Entries
		// outside the requested range must retain the previously installed colors.
		const byte partial[] = { 19, 37, 83, 101, 149, 211 };
		active.set(partial, 42, 2);
		active.get(result, 0, Graphics::PALETTE_COUNT);
		for (uint i = 0; i < Graphics::PALETTE_SIZE; ++i) {
			const byte expected = i >= 42 * 3 && i < 44 * 3 ? partial[i - 42 * 3] : room[i];
			TS_ASSERT_EQUALS(result[i], expected);
		}

		byte range[6];
		active.get(range, 42, 2);
		for (uint i = 0; i < sizeof(range); ++i)
			TS_ASSERT_EQUALS(range[i], partial[i]);

		const byte last[] = { 1, 2, 3 };
		active.set(last, 255, 1);
		active.get(range, 255, 1);
		for (uint i = 0; i < sizeof(last); ++i)
			TS_ASSERT_EQUALS(range[i], last[i]);

		range[0] = 123;
		active.set(range, Graphics::PALETTE_COUNT, 0);
		active.get(range, Graphics::PALETTE_COUNT, 0);
		TS_ASSERT_EQUALS(range[0], 123);
	}

	void testNativeColorsUseInstalledPalette() {
		Sherlock::RoseTattooHiresPalette active;
		byte room[Graphics::PALETTE_SIZE] = {};
		room[7 * 3] = 200;
		room[7 * 3 + 1] = 8;
		room[7 * 3 + 2] = 16;
		active.set(room, 0, Graphics::PALETTE_COUNT);

		// Simulate a separate darts palette while the game retains its room map.
		byte darts[Graphics::PALETTE_SIZE] = {};
		darts[7 * 3] = 17;
		darts[7 * 3 + 1] = 146;
		darts[7 * 3 + 2] = 230;
		darts[255 * 3] = 251;
		active.set(darts, 0, Graphics::PALETTE_COUNT);
		const Graphics::PixelFormat rgba = Graphics::PixelFormat::createFormatRGBA32();
		const Graphics::PixelFormat rgb565(2, 5, 6, 5, 0, 11, 5, 0, 0);
		const Graphics::PixelFormat clut8 = Graphics::PixelFormat::createFormatCLUT8();

		// The same conversion is used for the native fallback, foreground diff,
		// and native sprite replay. Replay can additionally request zero alpha.
		TS_ASSERT_EQUALS(active.color(rgba, 7), rgba.RGBToColor(17, 146, 230));
		TS_ASSERT_EQUALS(active.color(rgb565, 7), rgb565.RGBToColor(17, 146, 230));
		TS_ASSERT_EQUALS(active.color(rgba, 7, 0), rgba.ARGBToColor(0, 17, 146, 230));
		TS_ASSERT_EQUALS(active.color(clut8, 7), 7);
		TS_ASSERT_EQUALS(active.color(rgba, 255), rgba.RGBToColor(251, 0, 0));
		TS_ASSERT_EQUALS(active.color(clut8, 255), 255);
		TS_ASSERT_DIFFERS(active.color(rgba, 7), rgba.RGBToColor(room[21], room[22], room[23]));

		// Subsequent palette changes and restoration are visible immediately.
		const byte faded[] = { 0, 0, 0 };
		active.set(faded, 7, 1);
		TS_ASSERT_EQUALS(active.color(rgba, 7), rgba.RGBToColor(0, 0, 0));
		active.set(room, 0, Graphics::PALETTE_COUNT);
		TS_ASSERT_EQUALS(active.color(rgba, 7), rgba.RGBToColor(200, 8, 16));
	}

	void testBlackRequiresEveryInstalledComponentToBeZero() {
		Sherlock::RoseTattooHiresPalette active;
		TS_ASSERT(active.isBlack());

		const byte blue[] = { 0, 0, 1 };
		const byte black[] = { 0, 0, 0 };
		active.set(blue, 255, 1);
		TS_ASSERT(!active.isBlack());
		active.set(black, 0, 1);
		TS_ASSERT(!active.isBlack());
		active.set(black, 255, 1);
		TS_ASSERT(active.isBlack());

		const byte red[] = { 1, 0, 0 };
		active.set(red, 0, 1);
		TS_ASSERT(!active.isBlack());
		active.set(black, 0, 1);
		TS_ASSERT(active.isBlack());
		active.set(black, Graphics::PALETTE_COUNT, 0);
		TS_ASSERT(active.isBlack());

		// Even a single nonzero component at any palette index must resume
		// normal presentation, including partial palette updates.
		for (uint entry = 0; entry < Graphics::PALETTE_COUNT; ++entry) {
			for (uint component = 0; component < 3; ++component) {
				byte one[] = { 0, 0, 0 };
				one[component] = 1;
				active.set(one, entry, 1);
				TS_ASSERT(!active.isBlack());
				active.set(black, entry, 1);
				TS_ASSERT(active.isBlack());
			}
		}
	}
};

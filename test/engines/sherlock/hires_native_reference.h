#include <cxxtest/TestSuite.h>

#include "sherlock/hires_native_reference.h"

class SherlockHiresNativeReferenceTestSuite : public CxxTest::TestSuite {
public:
	void testScrolledFinalFramebufferIsCopiedWithoutSceneOrUiInference() {
		const Graphics::PixelFormat rgba = Graphics::PixelFormat::createFormatRGBA32();
		Graphics::ManagedSurface world(6, 3, Graphics::PixelFormat::createFormatCLUT8());
		world.clear(42);
		// Already-composited indices: a scene draw, two mask results, and
		// widget/text pixels, including one equal to the old background.
		const byte rows[2][4] = { { 1, 2, 3, 42 }, { 236, 5, 6, 7 } };
		for (int y = 0; y < 2; ++y)
			for (int x = 0; x < 4; ++x)
				world.setPixel(x + 1, y + 1, rows[y][x]);
		Graphics::ManagedSurface viewport(world, Common::Rect(1, 1, 5, 3));
		Graphics::ManagedSurface output(12, 6, rgba);
		Sherlock::RoseTattooHiresPalette palette;
		for (int i = 0; i < 256; ++i) {
			const byte color[] = { (byte)i, (byte)(255 - i), (byte)(i / 2) };
			palette.set(color, i, 1);
		}
		Sherlock::roseTattooNativeReferenceFrame(output, viewport, palette, 3);
		for (int y = 0; y < output.h; ++y) {
			for (int x = 0; x < output.w; ++x) {
				const byte index = rows[y / 3][x / 3];
				TS_ASSERT_EQUALS(output.getPixel(x, y), palette.color(rgba, index));
			}
		}
	}

	void testNextFrameAndInstalledPaletteReplaceEveryOutputPixel() {
		Graphics::ManagedSurface screen(3, 2, Graphics::PixelFormat::createFormatCLUT8());
		Graphics::ManagedSurface output(6, 4, Graphics::PixelFormat::createFormatRGBA32());
		Sherlock::RoseTattooHiresPalette palette;
		const byte first[] = { 10, 20, 30 }, next[] = { 90, 80, 70 };
		palette.set(first, 9, 1);
		screen.clear(9);
		Sherlock::roseTattooNativeReferenceFrame(output, screen, palette, 2);
		palette.set(next, 9, 1);
		screen.setPixel(2, 1, 0);
		Sherlock::roseTattooNativeReferenceFrame(output, screen, palette, 2);
		for (int y = 0; y < output.h; ++y)
			for (int x = 0; x < output.w; ++x)
				TS_ASSERT_EQUALS(output.getPixel(x, y), palette.color(output.format, x >= 4 && y >= 2 ? 0 : 9));
	}

	void testClutOutputPreservesIndices() {
		Graphics::ManagedSurface screen(2, 1, Graphics::PixelFormat::createFormatCLUT8());
		Graphics::ManagedSurface output(4, 2, Graphics::PixelFormat::createFormatCLUT8());
		Sherlock::RoseTattooHiresPalette palette;
		screen.setPixel(0, 0, 0);
		screen.setPixel(1, 0, 236);
		Sherlock::roseTattooNativeReferenceFrame(output, screen, palette, 2);
		for (int y = 0; y < output.h; ++y)
			for (int x = 0; x < output.w; ++x)
				TS_ASSERT_EQUALS(output.getPixel(x, y), x < 2 ? 0 : 236);
	}

	void testRgb565UsesInstalledPalette() {
		const Graphics::PixelFormat rgb565(2, 5, 6, 5, 0, 11, 5, 0, 0);
		Graphics::ManagedSurface screen(1, 1, Graphics::PixelFormat::createFormatCLUT8());
		Graphics::ManagedSurface output(2, 2, rgb565);
		Sherlock::RoseTattooHiresPalette palette;
		const byte color[] = { 127, 69, 211 };
		palette.set(color, 123, 1);
		screen.setPixel(0, 0, 123);
		Sherlock::roseTattooNativeReferenceFrame(output, screen, palette, 2);
		for (int y = 0; y < output.h; ++y)
			for (int x = 0; x < output.w; ++x)
				TS_ASSERT_EQUALS(output.getPixel(x, y), rgb565.RGBToColor(127, 69, 211));
	}
};

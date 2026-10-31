#include <cxxtest/TestSuite.h>

#include "graphics/managed_surface.h"
#include "sherlock/hires_sprite.h"

class SherlockHiresSpriteTestSuite : public CxxTest::TestSuite {
public:
	void testStraightAlphaOverlappingSprites() {
		const Graphics::PixelFormat fmt = Graphics::PixelFormat::createFormatRGBA32();
		const uint32 red = fmt.ARGBToColor(128, 255, 0, 0);
		const uint32 blue = fmt.ARGBToColor(128, 0, 0, 255);
		const uint32 transparent = fmt.ARGBToColor(0, 23, 45, 67);
		TS_ASSERT_EQUALS(Sherlock::hiresSpriteSourceOver(fmt, transparent, red), red);
		TS_ASSERT_EQUALS(Sherlock::hiresSpriteSourceOver(fmt, red, transparent), red);
		TS_ASSERT_EQUALS(Sherlock::hiresSpriteSourceOver(fmt, fmt.ARGBToColor(255, 1, 2, 3), red),
			fmt.ARGBToColor(255, 1, 2, 3));

		byte a, r, g, b;
		fmt.colorToARGB(Sherlock::hiresSpriteSourceOver(fmt, blue, red), a, r, g, b);
		TS_ASSERT_EQUALS(a, 192);
		TS_ASSERT_EQUALS(r, 85);
		TS_ASSERT_EQUALS(g, 0);
		TS_ASSERT_EQUALS(b, 170);
	}

	void testNativeCoverageMatchesScaledFlippedBlit() {
		// Includes index 0 (opaque) and 255 (transparent), and destination
		// sizes that expose fixed-point versus simple-ratio rounding.
		Graphics::Surface source;
		source.create(7, 3, Graphics::PixelFormat::createFormatCLUT8());
		for (int y = 0; y < source.h; ++y)
			for (int x = 0; x < source.w; ++x)
				source.setPixel(x, y, (x + y) % 3 == 0 ? 255 : (x + y == 1 ? 0 : x + y));

		for (int width = 2; width <= 12; ++width) {
			for (int height = 1; height <= 6; ++height) {
				for (int flip = 0; flip < 2; ++flip) {
					Graphics::ManagedSurface destination(width, height, Graphics::PixelFormat::createFormatCLUT8());
					destination.clear(200);
					destination.transBlitFrom(source, Common::Rect(source.w, source.h), Common::Rect(width, height), 255, flip);
					for (int y = 0; y < height; ++y) {
						for (int x = 0; x < width; ++x) {
							const int sx = Sherlock::hiresNativeSample(x, source.w, width, flip);
							const int sy = Sherlock::hiresNativeSample(y, source.h, height, false);
							const uint32 pixel = source.getPixel(sx, sy);
							TS_ASSERT_EQUALS(destination.getPixel(x, y), pixel == 255 ? 200 : pixel);
						}
					}
				}
			}
		}
		source.free();
	}
};

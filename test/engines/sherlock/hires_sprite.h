#include <cxxtest/TestSuite.h>

#include "graphics/managed_surface.h"
#include "sherlock/hires_sprite.h"

class SherlockHiresSpriteTestSuite : public CxxTest::TestSuite {
private:
	uint32 sample(const Graphics::Surface &source, int x, int y, int width, int height, bool flipped = false) {
		return Sherlock::hiresSpriteBilinearSample(source,
			Sherlock::hiresSpriteFilterAxis(x, source.w, width, flipped),
			Sherlock::hiresSpriteFilterAxis(y, source.h, height, false));
	}

	uint32 nearest(const Graphics::Surface &source, int x, int y, int width, int height, bool flipped = false) {
		return source.getPixel(Sherlock::hiresSpriteNearestSample(x, source.w, width, flipped),
			Sherlock::hiresSpriteNearestSample(y, source.h, height, false));
	}

	uint32 edgeSample(const Graphics::Surface &source, int x, int y, int width, int height, bool flipped = false) {
		return Sherlock::hiresSpriteEdgeSample(source.format, nearest(source, x, y, width, height, flipped),
			sample(source, x, y, width, height, flipped));
	}

	void assertARGB(const Graphics::PixelFormat &format, uint32 pixel, byte expectedA, byte expectedR,
			byte expectedG, byte expectedB) {
		byte a, r, g, b;
		format.colorToARGB(pixel, a, r, g, b);
		TS_ASSERT_EQUALS(a, expectedA);
		TS_ASSERT_EQUALS(r, expectedR);
		TS_ASSERT_EQUALS(g, expectedG);
		TS_ASSERT_EQUALS(b, expectedB);
	}

public:
	void testEdgeKeepsOpaqueInteriorAndFiltersAlphaBoundaries() {
		Graphics::Surface source;
		const Graphics::PixelFormat fmt = Graphics::PixelFormat::createFormatRGBA32();
		source.create(2, 1, fmt);
		source.setPixel(0, 0, fmt.ARGBToColor(255, 255, 0, 0));
		source.setPixel(1, 0, fmt.ARGBToColor(255, 0, 0, 255));
		TS_ASSERT_EQUALS(edgeSample(source, 1, 0, 4, 1), source.getPixel(0, 0));
		TS_ASSERT_DIFFERS(edgeSample(source, 1, 0, 4, 1), sample(source, 1, 0, 4, 1));
		TS_ASSERT_EQUALS(edgeSample(source, 1, 0, 4, 1, true), source.getPixel(1, 0));

		// Hidden cyan RGB must not leak into interpolated red, whether the
		// nearest texel is opaque or transparent.
		source.setPixel(1, 0, fmt.ARGBToColor(0, 0, 255, 255));
		assertARGB(fmt, edgeSample(source, 1, 0, 4, 1), 191, 255, 0, 0);
		assertARGB(fmt, edgeSample(source, 2, 0, 4, 1), 64, 255, 0, 0);
		assertARGB(fmt, Sherlock::hiresSpriteSourceOver(fmt, edgeSample(source, 0, 0, 1, 1),
			fmt.ARGBToColor(255, 0, 0, 255)), 255, 128, 0, 127);
		TS_ASSERT_EQUALS(edgeSample(source, 1, 0, 4, 1), sample(source, 1, 0, 4, 1));
		TS_ASSERT_EQUALS(edgeSample(source, 2, 0, 4, 1), sample(source, 2, 0, 4, 1));
		source.setPixel(1, 0, fmt.ARGBToColor(128, 0, 0, 255));
		TS_ASSERT_EQUALS(edgeSample(source, 1, 0, 4, 1), sample(source, 1, 0, 4, 1));
		TS_ASSERT_EQUALS(edgeSample(source, 2, 0, 4, 1), sample(source, 2, 0, 4, 1));
		source.free();
	}

	void testEdgeChecksBothNearestAndInterpolatedAlpha() {
		Graphics::Surface source;
		const Graphics::PixelFormat fmt = Graphics::PixelFormat::createFormatRGBA32();
		source.create(3, 1, fmt);
		source.setPixel(0, 0, fmt.ARGBToColor(0, 1, 2, 3));
		source.setPixel(1, 0, fmt.ARGBToColor(255, 20, 40, 60));
		source.setPixel(2, 0, fmt.ARGBToColor(0, 4, 5, 6));
		// At this reduction nearest is transparent, but the center-aligned
		// bilinear sample is fully opaque. It must remain bilinear.
		TS_ASSERT_EQUALS(edgeSample(source, 0, 0, 1, 1), source.getPixel(1, 0));
		source.setPixel(0, 0, fmt.ARGBToColor(255, 1, 2, 3));
		source.setPixel(1, 0, fmt.ARGBToColor(0, 20, 40, 60));
		TS_ASSERT_EQUALS(edgeSample(source, 0, 0, 1, 1), source.getPixel(1, 0));
		source.free();
	}

	void testEdgeOneToOneExactWithFlip() {
		Graphics::Surface source;
		const Graphics::PixelFormat fmt = Graphics::PixelFormat::createFormatRGBA32();
		source.create(5, 3, fmt);
		for (int y = 0; y < source.h; ++y)
			for (int x = 0; x < source.w; ++x)
				source.setPixel(x, y, fmt.ARGBToColor((x + y) % 3 == 0 ? 0 : (x * 49 + y * 5), x * 40, y * 60, x + y));
		for (int y = 0; y < source.h; ++y)
			for (int x = 0; x < source.w; ++x) {
				TS_ASSERT_EQUALS(edgeSample(source, x, y, source.w, source.h), source.getPixel(x, y));
				TS_ASSERT_EQUALS(edgeSample(source, x, y, source.w, source.h, true), source.getPixel(source.w - 1 - x, y));
			}
		source.free();
	}

	void testEdgeAcrossDimensionsFlipsAndOverlappingLayers() {
		Graphics::Surface source;
		const Graphics::PixelFormat fmt = Graphics::PixelFormat::createFormatRGBA32();
		const uint32 background = fmt.ARGBToColor(160, 10, 100, 200);
		source.create(7, 3, fmt);
		for (int y = 0; y < source.h; ++y)
			for (int x = 0; x < source.w; ++x)
				source.setPixel(x, y, fmt.ARGBToColor(x % 3 == 0 ? 0 : (x % 3 == 1 ? 128 : 255), x * 30, y * 70, x * y * 10));
		for (int width = 1; width <= 12; ++width) {
			for (int height = 1; height <= 8; ++height) {
				for (int flip = 0; flip < 2; ++flip) {
					for (int y = 0; y < height; ++y) {
						for (int x = 0; x < width; ++x) {
							const uint32 n = nearest(source, x, y, width, height, flip);
							const uint32 b = sample(source, x, y, width, height, flip);
							const uint32 e = edgeSample(source, x, y, width, height, flip);
							byte na, nr, ng, nb, ba, br, bg, bb, ea, er, eg, eb;
							fmt.colorToARGB(n, na, nr, ng, nb);
							fmt.colorToARGB(b, ba, br, bg, bb);
							fmt.colorToARGB(e, ea, er, eg, eb);
							TS_ASSERT_EQUALS(ea, ba);
							const uint32 expected = na == 255 && ba == 255 ? n : b;
							TS_ASSERT_EQUALS(e, expected);
							TS_ASSERT_EQUALS(Sherlock::hiresSpriteSourceOver(fmt, e, background),
								Sherlock::hiresSpriteSourceOver(fmt, expected, background));
						}
					}
				}
			}
		}
		source.free();
	}

	void testEdgeClippingUsesOriginalDrawCoordinates() {
		Graphics::Surface source;
		const Graphics::PixelFormat fmt = Graphics::PixelFormat::createFormatRGBA32();
		source.create(5, 4, fmt);
		for (int y = 0; y < source.h; ++y)
			for (int x = 0; x < source.w; ++x)
				source.setPixel(x, y, fmt.ARGBToColor(31 * (x + y), x * 40, y * 50, 5));
		const int width = 9, height = 7, viewportW = 6, viewportH = 5;
		for (int flip = 0; flip < 2; ++flip) {
			for (int left = -width; left <= viewportW; left += 3) {
				for (int top = -height; top <= viewportH; top += 2) {
					Graphics::ManagedSurface viewport(viewportW, viewportH, fmt);
					viewport.clear(0);
					for (int sy = 0; sy < height; ++sy)
						for (int sx = 0; sx < width; ++sx) {
							const int dx = left + sx, dy = top + sy;
							if (dx >= 0 && dx < viewportW && dy >= 0 && dy < viewportH)
								viewport.setPixel(dx, dy, edgeSample(source, sx, sy, width, height, flip));
						}
					for (int dy = 0; dy < viewportH; ++dy)
						for (int dx = 0; dx < viewportW; ++dx) {
							const int sx = dx - left, sy = dy - top;
							const uint32 expected = sx >= 0 && sx < width && sy >= 0 && sy < height ?
								edgeSample(source, sx, sy, width, height, flip) : 0;
							TS_ASSERT_EQUALS(viewport.getPixel(dx, dy), expected);
						}
				}
			}
		}
		source.free();
	}

	void testBilinearOneToOneExactIncludingTransparentPixelsAndFlip() {
		Graphics::Surface source;
		const Graphics::PixelFormat fmt = Graphics::PixelFormat::createFormatRGBA32();
		source.create(5, 3, fmt);
		for (int y = 0; y < source.h; ++y) {
			for (int x = 0; x < source.w; ++x) {
				const byte alpha = (x + y) % 3 == 0 ? 0 : (x * 41 + y * 17);
				source.setPixel(x, y, fmt.ARGBToColor(alpha, x * 40, y * 60, x + y));
			}
		}
		for (int y = 0; y < source.h; ++y) {
			for (int x = 0; x < source.w; ++x) {
				TS_ASSERT_EQUALS(sample(source, x, y, source.w, source.h), source.getPixel(x, y));
				TS_ASSERT_EQUALS(sample(source, x, y, source.w, source.h, true), source.getPixel(source.w - 1 - x, y));
			}
		}
		source.free();
	}

	void testBilinearOpaqueGradientAndEdgeClamping() {
		Graphics::Surface source;
		const Graphics::PixelFormat fmt = Graphics::PixelFormat::createFormatRGBA32();
		source.create(2, 2, fmt);
		source.setPixel(0, 0, fmt.ARGBToColor(255, 0, 0, 0));
		source.setPixel(1, 0, fmt.ARGBToColor(255, 255, 0, 0));
		source.setPixel(0, 1, fmt.ARGBToColor(255, 0, 255, 0));
		source.setPixel(1, 1, fmt.ARGBToColor(255, 255, 255, 0));
		assertARGB(fmt, sample(source, 0, 0, 1, 1), 255, 128, 128, 0);
		assertARGB(fmt, sample(source, 1, 0, 4, 4), 255, 64, 0, 0);
		assertARGB(fmt, sample(source, 2, 3, 4, 4), 255, 191, 255, 0);
		TS_ASSERT_EQUALS(sample(source, 0, 0, 4, 4), source.getPixel(0, 0));
		TS_ASSERT_EQUALS(sample(source, 3, 3, 4, 4), source.getPixel(1, 1));
		source.free();
	}

	void testBilinearPremultipliedAlphaAndTransparentColors() {
		Graphics::Surface source;
		const Graphics::PixelFormat fmt = Graphics::PixelFormat::createFormatRGBA32();
		source.create(2, 1, fmt);
		source.setPixel(0, 0, fmt.ARGBToColor(255, 255, 0, 0));
		source.setPixel(1, 0, fmt.ARGBToColor(0, 0, 255, 255));
		const uint32 midpoint = sample(source, 0, 0, 1, 1);
		assertARGB(fmt, midpoint, 128, 255, 0, 0);
		assertARGB(fmt, Sherlock::hiresSpriteSourceOver(fmt, midpoint, fmt.ARGBToColor(255, 0, 0, 0)),
			255, 128, 0, 0);
		assertARGB(fmt, Sherlock::hiresSpriteSourceOver(fmt, midpoint, fmt.ARGBToColor(255, 255, 255, 255)),
			255, 255, 127, 127);

		source.setPixel(0, 0, fmt.ARGBToColor(128, 255, 0, 0));
		source.setPixel(1, 0, fmt.ARGBToColor(255, 0, 0, 255));
		assertARGB(fmt, sample(source, 0, 0, 1, 1), 192, 85, 0, 170);
		source.setPixel(0, 0, fmt.ARGBToColor(0, 255, 0, 0));
		source.setPixel(1, 0, fmt.ARGBToColor(0, 0, 0, 255));
		assertARGB(fmt, sample(source, 0, 0, 1, 1), 0, 0, 0, 0);
		source.free();
	}

	void testBilinearSinglePixelAndMaximumWhite() {
		Graphics::Surface source;
		const Graphics::PixelFormat fmt = Graphics::PixelFormat::createFormatRGBA32();
		const uint32 white = fmt.ARGBToColor(255, 255, 255, 255);
		source.create(1, 1, fmt);
		source.setPixel(0, 0, white);
		for (int y = 0; y < 5; ++y)
			for (int x = 0; x < 7; ++x)
				TS_ASSERT_EQUALS(sample(source, x, y, 7, 5, true), white);
		source.free();
		source.create(2, 2, fmt);
		for (int y = 0; y < 2; ++y)
			for (int x = 0; x < 2; ++x)
				source.setPixel(x, y, white);
		// Exercises the largest possible accumulated alpha-weighted RGB.
		TS_ASSERT_EQUALS(sample(source, 1, 1, 3, 3), white);
		source.free();
	}

	void testBilinearFlipAndDimensions() {
		Graphics::Surface source;
		const Graphics::PixelFormat fmt = Graphics::PixelFormat::createFormatRGBA32();
		source.create(7, 3, fmt);
		for (int y = 0; y < source.h; ++y)
			for (int x = 0; x < source.w; ++x)
				source.setPixel(x, y, fmt.ARGBToColor((x * 37 + y * 29) % 256, x * 30, y * 70, x * y * 10));
		for (int width = 1; width <= 12; ++width) {
			for (int height = 1; height <= 8; ++height) {
				for (int y = 0; y < height; ++y) {
					for (int x = 0; x < width; ++x) {
						const Sherlock::HiresSpriteFilterAxis fx = Sherlock::hiresSpriteFilterAxis(x, source.w, width, false);
						const Sherlock::HiresSpriteFilterAxis fy = Sherlock::hiresSpriteFilterAxis(y, source.h, height, false);
						TS_ASSERT(fx.first >= 0 && fx.second < source.w);
						TS_ASSERT(fy.first >= 0 && fy.second < source.h);
						TS_ASSERT_EQUALS(sample(source, x, y, width, height, true),
							sample(source, width - 1 - x, y, width, height, false));
					}
				}
			}
		}
		source.free();
	}

	void testBilinearMatchesIndependentFloatingPointReference() {
		Graphics::Surface source;
		const Graphics::PixelFormat fmt = Graphics::PixelFormat::createFormatRGBA32();
		source.create(4, 3, fmt);
		for (int y = 0; y < source.h; ++y)
			for (int x = 0; x < source.w; ++x)
				source.setPixel(x, y, fmt.ARGBToColor(64 + (x * 61 + y * 37) % 192,
					(x * 91 + y * 39) % 256, (x * 47 + y * 89) % 256, (x * 31 + y * 101) % 256));
		for (int width = 1; width <= 9; ++width) {
			for (int height = 1; height <= 8; ++height) {
				for (int flip = 0; flip < 2; ++flip) {
					for (int y = 0; y < height; ++y) {
						for (int x = 0; x < width; ++x) {
							double u = ((flip ? width - 1 - x : x) + 0.5) * source.w / width - 0.5;
							double v = (y + 0.5) * source.h / height - 0.5;
							if (u < 0) u = 0;
							if (v < 0) v = 0;
							if (u > source.w - 1) u = source.w - 1;
							if (v > source.h - 1) v = source.h - 1;
							const int x0 = (int)u, y0 = (int)v;
							const int x1 = x0 + 1 < source.w ? x0 + 1 : x0;
							const int y1 = y0 + 1 < source.h ? y0 + 1 : y0;
							const double weights[] = { (1 - (u - x0)) * (1 - (v - y0)), (u - x0) * (1 - (v - y0)),
								(1 - (u - x0)) * (v - y0), (u - x0) * (v - y0) };
							const uint32 pixels[] = { source.getPixel(x0, y0), source.getPixel(x1, y0),
								source.getPixel(x0, y1), source.getPixel(x1, y1) };
							double alpha = 0, red = 0, green = 0, blue = 0;
							for (int i = 0; i < 4; ++i) {
								byte a, r, g, b;
								fmt.colorToARGB(pixels[i], a, r, g, b);
								alpha += weights[i] * a;
								red += weights[i] * a * r;
								green += weights[i] * a * g;
								blue += weights[i] * a * b;
							}
							byte a, r, g, b;
							fmt.colorToARGB(sample(source, x, y, width, height, flip), a, r, g, b);
							// The implementation uses 8-bit fractions; allow for that quantization.
							TS_ASSERT_DELTA((double)a, alpha, 1.0);
							TS_ASSERT_DELTA((double)r, red / alpha, 2.0);
							TS_ASSERT_DELTA((double)g, green / alpha, 2.0);
							TS_ASSERT_DELTA((double)b, blue / alpha, 2.0);
						}
					}
				}
			}
		}
		source.free();
	}

	void testBilinearClippingDoesNotChangeUnclippedGeometry() {
		Graphics::Surface source;
		const Graphics::PixelFormat fmt = Graphics::PixelFormat::createFormatRGBA32();
		source.create(5, 4, fmt);
		for (int y = 0; y < source.h; ++y)
			for (int x = 0; x < source.w; ++x)
				source.setPixel(x, y, fmt.ARGBToColor(31 * (x + y), x * 40, y * 50, 5));
		const int width = 9, height = 7, viewportW = 6, viewportH = 5;
		for (int flip = 0; flip < 2; ++flip) {
			for (int left = -width; left <= viewportW; left += 3) {
				for (int top = -height; top <= viewportH; top += 2) {
					Graphics::ManagedSurface viewport(viewportW, viewportH, fmt);
					viewport.clear(0);
					for (int sy = 0; sy < height; ++sy) {
						for (int sx = 0; sx < width; ++sx) {
							const int dx = left + sx, dy = top + sy;
							if (dx >= 0 && dx < viewportW && dy >= 0 && dy < viewportH)
								viewport.setPixel(dx, dy, sample(source, sx, sy, width, height, flip));
						}
					}
					for (int dy = 0; dy < viewportH; ++dy) {
						for (int dx = 0; dx < viewportW; ++dx) {
							const int sx = dx - left, sy = dy - top;
							const uint32 expected = sx >= 0 && sx < width && sy >= 0 && sy < height ?
								sample(source, sx, sy, width, height, flip) : 0;
							TS_ASSERT_EQUALS(viewport.getPixel(dx, dy), expected);
						}
					}
				}
			}
		}
		source.free();
	}

	void testOverrideNearestMappingIsUnchanged() {
		for (int source = 1; source <= 25; ++source)
			for (int destination = 1; destination <= 20; ++destination)
				for (int x = 0; x < destination; ++x) {
					TS_ASSERT_EQUALS(Sherlock::hiresSpriteNearestSample(x, source, destination, false), x * source / destination);
					TS_ASSERT_EQUALS(Sherlock::hiresSpriteNearestSample(x, source, destination, true), source - 1 - x * source / destination);
				}
	}

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

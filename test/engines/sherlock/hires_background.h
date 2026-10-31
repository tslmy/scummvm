#include <cxxtest/TestSuite.h>

#include "sherlock/hires_background.h"

class SherlockHiresBackgroundTestSuite : public CxxTest::TestSuite {
private:
	void compareCopy(const Graphics::PixelFormat &sourceFormat,
			const Graphics::PixelFormat &destFormat, int offsetX, int offsetY, bool colorKey) {
		Graphics::ManagedSurface source(9, 8, sourceFormat);
		Graphics::ManagedSurface oldResult(6, 5, destFormat);
		Graphics::ManagedSurface newResult(6, 5, destFormat);
		for (int y = 0; y < source.h; ++y) {
			for (int x = 0; x < source.w; ++x) {
				uint32 pixel = sourceFormat.isCLUT8() ? (x + y * source.w) :
					sourceFormat.RGBToColor(x * 23, y * 29, (x + y) * 13);
				source.setPixel(x, y, pixel);
			}
		}
		if (colorKey)
			source.setTransparentColor(source.getPixel(2, 2));
		oldResult.clear(0);
		Common::Rect from(offsetX, offsetY, offsetX + oldResult.w, offsetY + oldResult.h);
		from.clip(source.getBounds());
		if (!from.isEmpty()) {
			Common::Rect to = from;
			to.translate(-offsetX, -offsetY);
			oldResult.blitFrom(source, from, to);
		}
		Sherlock::copyRoseTattooOpaqueBackground(newResult, source, offsetX, offsetY);
		for (int y = 0; y < oldResult.h; ++y)
			for (int x = 0; x < oldResult.w; ++x)
				TS_ASSERT_EQUALS(newResult.getPixel(x, y), oldResult.getPixel(x, y));
	}

public:
	void testOpaqueCopyMatchesOldBlitForClippingAndFormats() {
		const Graphics::PixelFormat formats[] = {
			Graphics::PixelFormat::createFormatRGBA32(),
			Graphics::PixelFormat(2, 5, 6, 5, 0, 11, 5, 0, 0),
			Graphics::PixelFormat::createFormatCLUT8()
		};
		const int offsets[][2] = {{0, 0}, {2, 1}, {-2, -1}, {7, 6}, {-9, 0}, {9, 0}, {0, 8}};
		for (uint f = 0; f < ARRAYSIZE(formats); ++f)
			for (uint i = 0; i < ARRAYSIZE(offsets); ++i)
				compareCopy(formats[f], formats[f], offsets[i][0], offsets[i][1], false);
	}

	void testTransparentKeyAndDifferentFormatsUseOriginalBlit() {
		const Graphics::PixelFormat rgba = Graphics::PixelFormat::createFormatRGBA32();
		compareCopy(rgba, rgba, 1, 1, true);
		compareCopy(rgba, Graphics::PixelFormat(2, 5, 6, 5, 0, 11, 5, 0, 0), -1, 1, false);
	}
};

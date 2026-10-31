#include <cxxtest/TestSuite.h>

#include "common/memstream.h"
#include "sherlock/hires_palette_sprite.h"
#include "sherlock/hires_resource.h"

class SherlockHiresPaletteSpriteTestSuite : public CxxTest::TestSuite {
private:
	struct TestOwner {
		const int *first, *second;
		int indexOf(const int *frame) const {
			return frame == first ? 0 : frame == second ? 1 : -1;
		}
	};

	class ShortReadStream : public Common::MemoryReadStream {
	public:
		ShortReadStream(const byte *data, uint32 size) : Common::MemoryReadStream(data, size) {}
		uint32 read(void *, uint32) override { return 0; }
	};

	void addPixels(Sherlock::RoseTattooPersonIndices &indices, const byte *pixels, int width, int height) {
		Graphics::Surface surface;
		surface.create(width, height, Graphics::PixelFormat::createFormatCLUT8());
		for (int y = 0; y < height; ++y)
			for (int x = 0; x < width; ++x)
				surface.setPixel(x, y, pixels[y * width + x]);
		TS_ASSERT(indices.add(surface));
		surface.free();
	}

public:
	void testMainAndAlternateOwnersKeepTheirFrameIndices() {
		int mainFrames[2] = {}, alternateFrames[2] = {}, unrelated = 0;
		TestOwner main = { &mainFrames[0], &mainFrames[1] };
		TestOwner alternate = { &alternateFrames[0], &alternateFrames[1] };
		int index = -1;
		TS_ASSERT_EQUALS(Sherlock::roseTattooPersonFrameOwner(&main, &alternate, &mainFrames[1], index), &main);
		TS_ASSERT_EQUALS(index, 1);
		TS_ASSERT_EQUALS(Sherlock::roseTattooPersonFrameOwner(&main, &alternate, &alternateFrames[0], index), &alternate);
		TS_ASSERT_EQUALS(index, 0);
		TS_ASSERT_EQUALS(Sherlock::roseTattooPersonFrameOwner(&main, &alternate, &unrelated, index), (TestOwner *)nullptr);
		TS_ASSERT_EQUALS(index, -1);
	}

	void testUnionIncludesZeroAndNineButNotTransparency() {
		Sherlock::RoseTattooPersonIndices indices;
		const byte frame0[] = { 0, 255, 0, 255 };
		const byte frame1[] = { 9, 255, 9, 0 };
		addPixels(indices, frame0, 2, 2);
		addPixels(indices, frame1, 2, 2);
		TS_ASSERT_EQUALS(indices.bits[0], 1);
		TS_ASSERT_EQUALS(indices.bits[1], 2);
		for (uint i = 2; i < 32; ++i)
			TS_ASSERT_EQUALS(indices.bits[i], 0);

		Sherlock::RoseTattooHiresPalette palette;
		const byte zero[] = { 10, 11, 12 }, nine[] = { 90, 91, 92 }, transparent[] = { 1, 2, 3 };
		palette.set(zero, 0, 1);
		palette.set(nine, 9, 1);
		palette.set(transparent, 255, 1);
		const Common::Array<byte> payload = Sherlock::roseTattooPersonPalettePayload(indices, palette);
		const byte signature[] = { 'R', 'T', '-', 'P', 'A', 'L', '1', 0 };
		TS_ASSERT_EQUALS(payload.size(), 46);
		for (uint i = 0; i < sizeof(signature); ++i)
			TS_ASSERT_EQUALS(payload[i], signature[i]);
		TS_ASSERT_EQUALS(payload[8], 1);
		TS_ASSERT_EQUALS(payload[9], 2);
		for (uint i = 0; i < 3; ++i) {
			TS_ASSERT_EQUALS(payload[40 + i], zero[i]);
			TS_ASSERT_EQUALS(payload[43 + i], nine[i]);
		}
	}

	void testPartialUpdatesOnlyChangeUsedColors() {
		Sherlock::RoseTattooPersonIndices indices;
		const byte pixels[] = { 0, 9, 255 };
		addPixels(indices, pixels, 3, 1);
		Sherlock::RoseTattooHiresPalette palette;
		const byte colors[] = { 12, 34, 56, 78, 90, 123 };
		palette.set(colors, 9, 2);
		const Common::Array<byte> before = Sherlock::roseTattooPersonPalettePayload(indices, palette);
		const byte unrelated[] = { 1, 2, 3 };
		palette.set(unrelated, 10, 1);
		const Common::Array<byte> same = Sherlock::roseTattooPersonPalettePayload(indices, palette);
		TS_ASSERT_EQUALS(Sherlock::roseTattooPersonPaletteCacheDiscriminator(before),
			Sherlock::roseTattooPersonPaletteCacheDiscriminator(same));
		palette.set(unrelated, 9, 1);
		const Common::Array<byte> changed = Sherlock::roseTattooPersonPalettePayload(indices, palette);
		TS_ASSERT_DIFFERS(Sherlock::roseTattooPersonPaletteCacheDiscriminator(before),
			Sherlock::roseTattooPersonPaletteCacheDiscriminator(changed));
		TS_ASSERT_EQUALS(changed[40], 0); // index 0 retained from the installed palette
		TS_ASSERT_EQUALS(changed[43], 1);
		TS_ASSERT_EQUALS(changed[44], 2);
		TS_ASSERT_EQUALS(changed[45], 3);
	}

	void testSidecarIsExactAndFailsClosed() {
		Sherlock::RoseTattooPersonIndices indices;
		const byte pixels[] = { 0, 9, 255 };
		addPixels(indices, pixels, 3, 1);
		Sherlock::RoseTattooHiresPalette palette;
		Common::Array<byte> payload = Sherlock::roseTattooPersonPalettePayload(indices, palette);
		Common::MemoryReadStream valid(payload.begin(), payload.size());
		TS_ASSERT(Sherlock::roseTattooPersonPaletteMatches(&valid, payload));
		TS_ASSERT(!Sherlock::roseTattooPersonPaletteMatches(nullptr, payload));
		const Common::String qualified = "person_vgs/variants/source-s0a0/palettes/projection";
		TS_ASSERT_EQUALS(Sherlock::roseTattooPersonVerifiedResource(qualified, &valid, payload), qualified);
		TS_ASSERT(Sherlock::roseTattooPersonVerifiedResource(qualified, nullptr, payload).empty());
		TS_ASSERT(Sherlock::roseTattooPersonVerifiedResource("", &valid, payload).empty());
		Common::MemoryReadStream truncated(payload.begin(), payload.size() - 1);
		TS_ASSERT(!Sherlock::roseTattooPersonPaletteMatches(&truncated, payload));
		ShortReadStream unreadable(payload.begin(), payload.size());
		TS_ASSERT(!Sherlock::roseTattooPersonPaletteMatches(&unreadable, payload));
		Common::Array<byte> corrupt = payload;
		corrupt[8] ^= 1; // same length, but an incorrect used-index mask
		Common::MemoryReadStream badMask(corrupt.begin(), corrupt.size());
		TS_ASSERT(!Sherlock::roseTattooPersonPaletteMatches(&badMask, payload));
		corrupt = payload;
		corrupt[43] ^= 1; // same path and size, but a different installed color
		Common::MemoryReadStream badColor(corrupt.begin(), corrupt.size());
		TS_ASSERT(!Sherlock::roseTattooPersonPaletteMatches(&badColor, payload));
		// Empty means native replay; a legacy "person_vgs" must never replace it.
		TS_ASSERT(Sherlock::roseTattooPersonVerifiedResource(qualified, &badColor, payload).empty());
		corrupt = payload;
		corrupt.push_back(0);
		Common::MemoryReadStream trailing(corrupt.begin(), corrupt.size());
		TS_ASSERT(!Sherlock::roseTattooPersonPaletteMatches(&trailing, payload));
	}

	void testResourcePaletteAndDimensionsSeparateHitsAndMisses() {
#ifndef DISABLE_MD5
		const byte a[] = { 'a', 'b', 'c' }, b[] = { 'a', 'b', 'd' };
		Common::MemoryReadStream streamA(a, sizeof(a)), streamB(b, sizeof(b));
		const Common::String aKey = Sherlock::roseTattooHiresVariantKey("person_vgs", streamA, false, false);
		const Common::String bKey = Sherlock::roseTattooHiresVariantKey("person_vgs", streamB, false, false);
		Sherlock::RoseTattooPersonIndices indices;
		const byte pixels[] = { 0, 9, 255 };
		addPixels(indices, pixels, 3, 1);
		Sherlock::RoseTattooHiresPalette palette;
		const Common::Array<byte> first = Sherlock::roseTattooPersonPalettePayload(indices, palette);
		const byte color[] = { 1, 2, 3 };
		palette.set(color, 9, 1);
		const Common::Array<byte> second = Sherlock::roseTattooPersonPalettePayload(indices, palette);
		const Common::String ra = Sherlock::roseTattooPersonPaletteResource(aKey, first);
		const Common::String rb = Sherlock::roseTattooPersonPaletteResource(bKey, first);
		const Common::String rc = Sherlock::roseTattooPersonPaletteResource(aKey, second);
		TS_ASSERT(ra.hasPrefix("person_vgs/variants/900150983cd24fb0d6963f7d28e17f72-s0a0/palettes/"));
		TS_ASSERT_EQUALS(ra, "person_vgs/variants/900150983cd24fb0d6963f7d28e17f72-s0a0/palettes/bed25915af43c49e98193fbbc362209e");
		TS_ASSERT_EQUALS(ra.size(), aKey.size() + 10 + 32);
		const Common::String firstTag = Sherlock::roseTattooPersonPaletteCacheDiscriminator(first);
		const Common::String secondTag = Sherlock::roseTattooPersonPaletteCacheDiscriminator(second);
		const Common::String key = Sherlock::roseTattooHiresSpriteCacheKey(ra, 0, 100, 200, firstTag);
		TS_ASSERT_DIFFERS(key, Sherlock::roseTattooHiresSpriteCacheKey(rb, 0, 100, 200, firstTag));
		TS_ASSERT_DIFFERS(key, Sherlock::roseTattooHiresSpriteCacheKey(rc, 0, 100, 200, secondTag));
		TS_ASSERT_DIFFERS(key, Sherlock::roseTattooHiresSpriteCacheKey(ra, 0, 100, 200, secondTag));
		TS_ASSERT_DIFFERS(key, Sherlock::roseTattooHiresSpriteCacheKey(ra, 0, 102, 200, firstTag));
		TS_ASSERT_DIFFERS(key, Sherlock::roseTattooHiresSpriteCacheKey(ra, 1, 100, 200, firstTag));
		TS_ASSERT(!Sherlock::roseTattooHiresSpriteDimensionsMatch(99, 200, 100, 200));
		TS_ASSERT(Sherlock::roseTattooPersonPaletteResource("", first).empty());
#endif
	}
};

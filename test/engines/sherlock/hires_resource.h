#include <cxxtest/TestSuite.h>

#include "common/memstream.h"
#include "sherlock/hires_resource.h"
#include "sherlock/resources.h"

class SherlockHiresResourceTestSuite : public CxxTest::TestSuite {
private:
	class ShortReadStream : public Common::MemoryReadStream {
	public:
		ShortReadStream(const byte *data, uint32 size) : Common::MemoryReadStream(data, size) {}
		uint32 read(void *, uint32) override { return 0; }
	};

	Common::String selectedLibrary(Sherlock::LibraryIndexes &indexes) {
		for (Sherlock::LibraryIndexes::iterator i = indexes.begin(); i != indexes.end(); ++i)
			if (i->_value.contains("NOHAT.VGS"))
				return i->_key.toString('/');
		return Common::String();
	}

	void addLibrary(Sherlock::LibraryIndexes &indexes, const char *name, bool hasImage = false) {
		indexes[name] = Sherlock::LibraryIndex();
		if (hasImage)
			indexes[name]["NOHAT.VGS"] = Sherlock::LibraryEntry(0, 0, 3);
	}

public:
	void testAmbiguousNamesOnly() {
		const char *names[] = { "htuprigh_vgs", "jtdownrg_vgs", "jtright_vgs", "jtuprigh_vgs", "nohat_vgs" };
		for (uint i = 0; i < ARRAYSIZE(names); ++i)
			TS_ASSERT(Sherlock::isAmbiguousRoseTattooHiresResource(names[i]));
		TS_ASSERT(Sherlock::isAmbiguousRoseTattooHiresResource("NoHaT_VgS"));
		TS_ASSERT(!Sherlock::isAmbiguousRoseTattooHiresResource("htright_vgs"));
		TS_ASSERT(!Sherlock::isAmbiguousRoseTattooHiresResource("scene_001/room_001"));
		TS_ASSERT(!Sherlock::isAmbiguousRoseTattooHiresResource("scene_001/canim_000"));
		TS_ASSERT(!Sherlock::canUseRoseTattooHiresKey("nohat_vgs", true, false));
		TS_ASSERT(Sherlock::canUseRoseTattooHiresKey("nohat_vgs", true, true));
		TS_ASSERT(Sherlock::canUseRoseTattooHiresKey("nohat_vgs", false, false));
		TS_ASSERT(Sherlock::canUseRoseTattooHiresKey("watson_vgs", true, false));
		TS_ASSERT(Sherlock::canUseRoseTattooHiresKey("scene_001/room_001", true, false));
	}

	void testCompleteContentModeAndPosition() {
#ifndef DISABLE_MD5
		const byte a[] = { 'a', 'b', 'c' };
		const byte b[] = { 'a', 'b', 'd' }; // same size, different content
		Common::MemoryReadStream first(a, sizeof(a)), second(b, sizeof(b));
		TS_ASSERT(first.seek(2));
		const Common::String a00 = Sherlock::roseTattooHiresVariantKey("nohat_vgs", first, false, false);
		const Common::String b00 = Sherlock::roseTattooHiresVariantKey("nohat_vgs", second, false, false);
		TS_ASSERT_EQUALS(a00, "nohat_vgs/variants/900150983cd24fb0d6963f7d28e17f72-s0a0");
		TS_ASSERT_EQUALS(b00, "nohat_vgs/variants/4911e516e5aa21d327512e0c8b197616-s0a0");
		TS_ASSERT_DIFFERS(a00, b00);
		TS_ASSERT(Sherlock::canUseRoseTattooHiresKey(a00, true, false));
		TS_ASSERT_EQUALS(first.pos(), 2);
		TS_ASSERT_EQUALS(first.readByte(), 'c');
		TS_ASSERT_EQUALS(second.pos(), 0);
		TS_ASSERT_EQUALS(Sherlock::roseTattooHiresVariantKey("nohat_vgs", first, true, false),
			"nohat_vgs/variants/900150983cd24fb0d6963f7d28e17f72-s1a0");
		TS_ASSERT_EQUALS(Sherlock::roseTattooHiresVariantKey("nohat_vgs", first, false, true),
			"nohat_vgs/variants/900150983cd24fb0d6963f7d28e17f72-s0a1");
		TS_ASSERT_EQUALS(Sherlock::roseTattooHiresVariantKey("nohat_vgs", first, true, true),
			"nohat_vgs/variants/900150983cd24fb0d6963f7d28e17f72-s1a1");

		// Same dimensions must not share a hit or miss across resource variants.
		TS_ASSERT_DIFFERS(Sherlock::roseTattooHiresSpriteCacheKey(a00, 0, 100, 200),
			Sherlock::roseTattooHiresSpriteCacheKey(b00, 0, 100, 200));
		TS_ASSERT(Sherlock::roseTattooHiresSpriteDimensionsMatch(100, 200, 100, 200));
		TS_ASSERT(!Sherlock::roseTattooHiresSpriteDimensionsMatch(102, 200, 100, 200));
		TS_ASSERT(!Sherlock::roseTattooHiresSpriteDimensionsMatch(100, 202, 100, 200));
#endif
	}

	void testVgsDataSameAndDifferentDimensions() {
#ifndef DISABLE_MD5
		// Three valid raw one-frame VGS streams. The first pair has identical
		// dimensions but different pixels; the third changes the source width.
		const byte a[] = { 1, 0, 1, 0, 0, 0, 0, 0, 1, 2, 3, 255 };
		const byte b[] = { 1, 0, 1, 0, 0, 0, 0, 0, 1, 2, 4, 255 };
		const byte c[] = { 2, 0, 1, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 255 };
		Common::MemoryReadStream first(a, sizeof(a)), second(b, sizeof(b)), third(c, sizeof(c));
		const Common::String ka = Sherlock::roseTattooHiresVariantKey("jtright_vgs", first, false, false);
		const Common::String kb = Sherlock::roseTattooHiresVariantKey("jtright_vgs", second, false, false);
		const Common::String kc = Sherlock::roseTattooHiresVariantKey("jtright_vgs", third, false, false);
		TS_ASSERT_DIFFERS(ka, kb);
		TS_ASSERT_DIFFERS(ka, kc);
		const int firstWidth = first.readUint16LE() + 1, firstHeight = first.readUint16LE() + 1;
		const int secondWidth = second.readUint16LE() + 1, secondHeight = second.readUint16LE() + 1;
		const int thirdWidth = third.readUint16LE() + 1, thirdHeight = third.readUint16LE() + 1;
		TS_ASSERT_EQUALS(firstWidth, secondWidth);
		TS_ASSERT_EQUALS(firstHeight, secondHeight);
		TS_ASSERT(Sherlock::roseTattooHiresSpriteDimensionsMatch(firstWidth * 2, firstHeight * 2,
			secondWidth * 2, secondHeight * 2));
		TS_ASSERT(!Sherlock::roseTattooHiresSpriteDimensionsMatch(firstWidth * 2, firstHeight * 2,
			thirdWidth * 2, thirdHeight * 2));
#endif
	}

	void testIncompleteReadDoesNotReturnAKey() {
		const byte data[] = { 'a', 'b', 'c' };
		ShortReadStream stream(data, sizeof(data));
		TS_ASSERT(stream.seek(1));
		TS_ASSERT(Sherlock::roseTattooHiresVariantKey("nohat_vgs", stream, false, false).empty());
		TS_ASSERT_EQUALS(stream.pos(), 1);
	}

	void testActualLibraryIndexIterationCanSwitchVariant() {
#ifndef DISABLE_MD5
		Sherlock::LibraryIndexes indexes;
		addLibrary(indexes, "vgs.lib", true);
		addLibrary(indexes, "talk.lib");
		addLibrary(indexes, "walk.lib", true);
		addLibrary(indexes, "music.lib");
		addLibrary(indexes, "sound.lib");
		TS_ASSERT_EQUALS(selectedLibrary(indexes), "walk.lib");

		const byte walk[] = { 'a', 'b', 'c' }, vgs[] = { 'a', 'b', 'd' };
		Common::MemoryReadStream walkStream(walk, sizeof(walk)), vgsStream(vgs, sizeof(vgs));
		const Common::String first = Sherlock::roseTattooHiresVariantKey("nohat_vgs",
			selectedLibrary(indexes) == "walk.lib" ? walkStream : vgsStream, false, false);
		const char *speech[] = { "speech26.lib", "speech36.lib", "speech53.lib", "speech18.lib", "speech02.lib", "speech12.lib" };
		for (uint i = 0; i < ARRAYSIZE(speech); ++i)
			addLibrary(indexes, speech[i]);
		TS_ASSERT_EQUALS(selectedLibrary(indexes), "vgs.lib");
		const Common::String second = Sherlock::roseTattooHiresVariantKey("nohat_vgs",
			selectedLibrary(indexes) == "walk.lib" ? walkStream : vgsStream, false, false);
		TS_ASSERT_DIFFERS(first, second);
		TS_ASSERT_DIFFERS(Sherlock::roseTattooHiresSpriteCacheKey(first, 3, 134, 366),
			Sherlock::roseTattooHiresSpriteCacheKey(second, 3, 134, 366));
#endif
	}
};

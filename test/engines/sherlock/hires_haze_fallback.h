#include <cxxtest/TestSuite.h>

#include "sherlock/hires_haze_fallback.h"

class SherlockHiresHazeFallbackTestSuite : public CxxTest::TestSuite {
public:
	void testOptInSceneSelectionAndTransitions() {
		Sherlock::RoseTattooHazeFallback disabled;
		TS_ASSERT(!disabled.touches(18));
		TS_ASSERT(!disabled.select(18));
		TS_ASSERT(!disabled.active());
		Sherlock::RoseTattooHazeFallback enabled(true);
		const int haze[] = {7, 8, 18, 53, 68};
		for (unsigned i = 0; i < sizeof(haze) / sizeof(haze[0]); ++i) {
			TS_ASSERT(enabled.select(haze[i]));
			TS_ASSERT(enabled.active());
			TS_ASSERT(!enabled.select(haze[i]));
			TS_ASSERT(enabled.select(-1)); // map or exit
			TS_ASSERT(!enabled.active());
		}
		TS_ASSERT(!enabled.select(36));
		TS_ASSERT(!enabled.active());
		TS_ASSERT(enabled.select(7));
		TS_ASSERT(enabled.touches(8));
		TS_ASSERT(enabled.touches(-1));
		TS_ASSERT(!enabled.select(8)); // same policy, different haze room
		TS_ASSERT(enabled.select(36));
		TS_ASSERT(!enabled.touches(1));
		TS_ASSERT(enabled.touches(18));
	}

	void testEveryOtherSceneAndMapRemainEnhanced() {
		Sherlock::RoseTattooHazeFallback enabled(true);
		bool previous = false;
		for (int scene = -1; scene <= 103; ++scene) {
			const bool expected = scene == 7 || scene == 8 || scene == 18 || scene == 53 || scene == 68;
			TS_ASSERT_EQUALS(enabled.select(scene), expected != previous);
			TS_ASSERT_EQUALS(enabled.active(), expected);
			previous = expected;
		}
	}
};

#include <cxxtest/TestSuite.h>

#include "sherlock/rose_frame_pacing.h"

class SherlockRoseFramePacingTestSuite : public CxxTest::TestSuite {
public:
	void testPacingRequiresHiresMap() {
		TS_ASSERT(Sherlock::roseTattooMapFramePacing(2, true));
		TS_ASSERT(!Sherlock::roseTattooMapFramePacing(1, true));
		TS_ASSERT(!Sherlock::roseTattooMapFramePacing(2, false));
	}

	void testWaitNeverOvershootsTheFrameDeadline() {
		TS_ASSERT_EQUALS(Sherlock::roseTattooPollWait(100, 100, 30), 10);
		TS_ASSERT_EQUALS(Sherlock::roseTattooPollWait(123, 100, 30), 10);
		TS_ASSERT_EQUALS(Sherlock::roseTattooPollWait(124, 100, 30), 9);
		TS_ASSERT_EQUALS(Sherlock::roseTattooPollWait(132, 100, 30), 1);
		TS_ASSERT_EQUALS(Sherlock::roseTattooPollWait(133, 100, 30), 1);
		TS_ASSERT_EQUALS(Sherlock::roseTattooPollWait(200, 100, 30), 1);
	}

	void testClockWrapAndOtherRates() {
		TS_ASSERT_EQUALS(Sherlock::roseTattooPollWait(4, 0xfffffff0U, 30), 10);
		TS_ASSERT_EQUALS(Sherlock::roseTattooPollWait(14, 0xfffffff0U, 30), 3);
		TS_ASSERT_EQUALS(Sherlock::roseTattooPollWait(110, 100, 60), 6);
		TS_ASSERT_EQUALS(Sherlock::roseTattooPollWait(100, 100, 0), 10);
		TS_ASSERT_EQUALS(Sherlock::roseTattooPollWait(100, 100, 1001), 10);
	}

	void testSplitWaitStillSleepsTenMilliseconds() {
		for (uint32 elapsed = 0; elapsed < 50; ++elapsed) {
			const uint32 first = Sherlock::roseTattooPollWait(100 + elapsed, 100, 30);
			const uint32 second = first < 10 ? 10 - first : 0;
			TS_ASSERT_EQUALS(first + second, 10);
		}
	}
};

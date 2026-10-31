#include <cxxtest/TestSuite.h>

#include "sherlock/rose_mouse_input.h"

class SherlockRoseMouseInputTestSuite : public CxxTest::TestSuite {
public:
	void testMousePositionsMatchExistingCoordinateConversion() {
		const Common::Point pt(401, 603);
		TS_ASSERT_EQUALS(Sherlock::roseTattooMousePosition(pt, false, 1), pt);
		TS_ASSERT_EQUALS(Sherlock::roseTattooMousePosition(pt, false, 2), Common::Point(200, 301));
		TS_ASSERT_EQUALS(Sherlock::roseTattooMousePosition(pt, false, 3), Common::Point(133, 201));
		TS_ASSERT_EQUALS(Sherlock::roseTattooMousePosition(pt, true, 3), Common::Point(200, 301));
	}

	void testQuickClickIsDeliveredInOrderWithItsPositions() {
		Sherlock::RoseTattooMouseInput input;
		input.record(1, true, Common::Point(10, 20));
		input.record(1, false, Common::Point(12, 21));
		TS_ASSERT_EQUALS(input.physical(), 0);
		Common::Point position;
		TS_ASSERT(input.consume(position));
		TS_ASSERT_EQUALS(input.delivered(), 1);
		TS_ASSERT_EQUALS(position, Common::Point(10, 20));
		TS_ASSERT(input.consume(position));
		TS_ASSERT_EQUALS(input.delivered(), 0);
		TS_ASSERT_EQUALS(position, Common::Point(12, 21));
		TS_ASSERT(!input.consume(position));
	}

	void testButtonsAndRepeatedClicksKeepTheirOrder() {
		Sherlock::RoseTattooMouseInput input;
		input.record(1, true, Common::Point(1, 1));
		input.record(2, true, Common::Point(2, 2));
		input.record(1, false, Common::Point(3, 3));
		input.record(1, true, Common::Point(4, 4));
		input.record(2, false, Common::Point(5, 5));
		input.record(1, false, Common::Point(6, 6));
		Common::Point position;
		const int expected[] = {1, 3, 2, 3, 1, 0};
		for (int i = 0; i < 6; ++i) {
			TS_ASSERT(input.consume(position));
			TS_ASSERT_EQUALS(input.delivered(), expected[i]);
			TS_ASSERT_EQUALS(position, Common::Point(i + 1, i + 1));
		}
		TS_ASSERT_EQUALS(input.physical(), 0);
		TS_ASSERT(!input.consume(position));
	}

	void testDuplicateEventsAndClearDoNotLeaveStuckButtons() {
		Sherlock::RoseTattooMouseInput input;
		input.record(1, false, Common::Point(1, 1));
		input.record(1, true, Common::Point(2, 2));
		input.record(1, true, Common::Point(3, 3));
		Common::Point position;
		TS_ASSERT(input.consume(position));
		TS_ASSERT_EQUALS(position, Common::Point(2, 2));
		TS_ASSERT(!input.consume(position));
		input.record(1, false, Common::Point(4, 4));
		input.clear();
		TS_ASSERT_EQUALS(input.physical(), 0);
		TS_ASSERT_EQUALS(input.delivered(), 0);
		TS_ASSERT(!input.consume(position));
		input.record(1, false, Common::Point(5, 5));
		TS_ASSERT(!input.consume(position));
	}

	void testHeldButtonRemainsDownUntilItsReleaseIsConsumed() {
		Sherlock::RoseTattooMouseInput input;
		Common::Point position;
		input.record(1, true, Common::Point(10, 10));
		TS_ASSERT(input.consume(position));
		TS_ASSERT(!input.consume(position));
		TS_ASSERT_EQUALS(input.physical(), 1);
		TS_ASSERT_EQUALS(input.delivered(), 1);
		input.record(1, false, Common::Point(20, 20));
		TS_ASSERT_EQUALS(input.physical(), 0);
		TS_ASSERT_EQUALS(input.delivered(), 1);
		TS_ASSERT(input.consume(position));
		TS_ASSERT_EQUALS(input.delivered(), 0);
		TS_ASSERT_EQUALS(position, Common::Point(20, 20));
	}
};

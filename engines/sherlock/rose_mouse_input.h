/* ScummVM - Graphic Adventure Engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef SHERLOCK_ROSE_MOUSE_INPUT_H
#define SHERLOCK_ROSE_MOUSE_INPUT_H

#include "common/rect.h"
#include "common/queue.h"

namespace Sherlock {

// Match Events::pollEvents' existing screen-to-native coordinate mapping.
inline Common::Point roseTattooMousePosition(const Common::Point &position, bool doubled, int hiresScale) {
	if (doubled)
		return Common::Point(position.x / 2, position.y / 2);
	if (hiresScale > 1)
		return Common::Point(position.x / hiresScale, position.y / hiresScale);
	return position;
}

// Polling can happen several times while a frame is being drawn. Preserve
// button transitions until the game consumes them, including their positions,
// rather than reducing a quick press/release to an unchanged physical state.
class RoseTattooMouseInput {
private:
	struct Transition {
		int button;
		bool down;
		Common::Point position;
	};

	int _physical;
	int _delivered;
	Common::Queue<Transition> _pending;

public:
	RoseTattooMouseInput() : _physical(0), _delivered(0) {}

	void record(int button, bool down, const Common::Point &position) {
		if (((_physical & button) != 0) == down)
			return;
		if (down)
			_physical |= button;
		else
			_physical &= ~button;
		Transition transition = { button, down, position };
		_pending.push(transition);
	}

	bool consume(Common::Point &position) {
		if (_pending.empty())
			return false;
		const Transition transition = _pending.pop();
		if (transition.down)
			_delivered |= transition.button;
		else
			_delivered &= ~transition.button;
		position = transition.position;
		return true;
	}

	void clear() {
		_pending.clear();
		_physical = _delivered = 0;
	}

	int physical() const { return _physical; }
	int delivered() const { return _delivered; }
};

} // End of namespace Sherlock

#endif

/* ScummVM - Graphic Adventure Engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef SHERLOCK_ROSE_INTERPOLATION_H
#define SHERLOCK_ROSE_INTERPOLATION_H

#include "common/array.h"
#include "common/ptr.h"
#include "common/rect.h"
#include "common/str.h"
#include "graphics/managed_surface.h"

namespace Sherlock {

/**
 * Presentation-only smoothing of Rose Tattoo's scene sprites.
 *
 * The game animates in steps (about 10 per second) while the window is
 * presented about 30 times per second. This keeps the hires scene sprites of
 * the last two steps and composes in-between pictures: positions move from
 * the previous step to the current one, and a changed animation frame can be
 * cross-faded. The picture runs up to one step behind the game state and
 * ends each step exactly on it. Nothing here is read back by game logic.
 */
class RoseTattooInterpolator {
public:
	struct Sprite {
		const void *key;           // identity across steps; null is never matched
		Common::String signature;  // changes when the drawn frame changes
		float x, y;                // anchor, native room coordinates
		Common::Rect rect;         // placement in the scene layer (output px)
		Common::SharedPtr<Graphics::ManagedSurface> pixels; // RGBA32, size of rect
	};

	RoseTattooInterpolator() : _stepStart(0), _interval(0) {}

	/** Forget all history (room change). */
	void reset();
	/** The game is about to redraw all scene sprites. */
	void beginStep(uint32 now);
	/** One sprite of the current step, in draw order. */
	void add(const Sprite &sprite) { _current.push_back(sprite); }

	/**
	 * Compose the in-between scene layer for time `now` into `layer`: the
	 * current step's `sceneLayer` (used area `sceneRect`) with the areas that
	 * move or fade redrawn. `rect` receives the used area of `layer`.
	 *
	 * Returns false when `sceneLayer` itself is the right picture (step
	 * complete, no history, or nothing differs).
	 */
	bool compose(uint32 now, bool lerp, bool crossfade, int scale, const Graphics::ManagedSurface &sceneLayer,
		const Common::Rect &sceneRect, Graphics::ManagedSurface &layer, Common::Rect &rect) const;

private:
	Common::Array<Sprite> _previous, _current;
	uint32 _stepStart, _interval;

	const Sprite *findPrevious(const void *key) const;
};

} // End of namespace Sherlock

#endif

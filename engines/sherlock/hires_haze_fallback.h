/* ScummVM - Graphic Adventure Engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef SHERLOCK_HIRES_HAZE_FALLBACK_H
#define SHERLOCK_HIRES_HAZE_FALLBACK_H

namespace Sherlock {

inline bool roseTattooHazeFallbackScene(int scene) {
	return scene == 7 || scene == 8 || scene == 18 || scene == 53 || scene == 68;
}

class RoseTattooHazeFallback {
	bool _enabled;
	bool _active;

public:
	explicit RoseTattooHazeFallback(bool enabled = false) : _enabled(enabled), _active(false) {}
	bool active() const { return _active; }
	bool touches(int nextScene) const {
		return _active || (_enabled && roseTattooHazeFallbackScene(nextScene));
	}
	// Returns true only when the effective room policy changes. A map or
	// non-room transition must select a non-haze scene before it draws.
	bool select(int scene) {
		const bool active = _enabled && roseTattooHazeFallbackScene(scene);
		const bool changed = active != _active;
		_active = active;
		return changed;
	}
};

} // End of namespace Sherlock

#endif

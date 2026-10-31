/* ScummVM - Graphic Adventure Engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef SHERLOCK_ROSE_FRAME_PACING_H
#define SHERLOCK_ROSE_FRAME_PACING_H

#include "common/scummsys.h"

namespace Sherlock {

inline bool roseTattooMapFramePacing(int hiresScale, bool map) {
	return hiresScale > 1 && map;
}

// First part of the map loop's existing 10 ms sleep. This can wake at a frame
// deadline; the caller sleeps the remainder afterward. The 1 ms minimum
// still yields if rendering has already overrun the deadline.
inline uint32 roseTattooPollWait(uint32 now, uint32 priorFrame, uint32 frameRate) {
	if (!frameRate || frameRate > 1000)
		return 10;
	const uint32 interval = 1000 / frameRate;
	const uint32 elapsed = now - priorFrame; // unsigned wrap is intentional
	if (elapsed >= interval)
		return 1;
	const uint32 remaining = interval - elapsed;
	return remaining < 10 ? remaining : 10;
}

} // End of namespace Sherlock

#endif

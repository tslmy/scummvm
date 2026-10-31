/* ScummVM - Graphic Adventure Engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef SHERLOCK_HIRES_NATIVE_REFERENCE_H
#define SHERLOCK_HIRES_NATIVE_REFERENCE_H

#include "graphics/managed_surface.h"
#include "sherlock/hires_palette.h"

namespace Sherlock {

// Diagnostic only: the source is the already-composited native *screen*,
// not a room buffer. It includes masks, scrolling, scene and native UI draws.
// The backend-owned cursor is drawn separately.
inline void roseTattooNativeReferenceFrame(Graphics::ManagedSurface &dest,
		const Graphics::ManagedSurface &source, const RoseTattooHiresPalette &palette, int scale) {
	assert(scale > 1 && source.format.isCLUT8());
	assert(dest.w == source.w * scale && dest.h == source.h * scale);
	for (int y = 0; y < source.h; ++y) {
		const byte *row = (const byte *)source.getBasePtr(0, y);
		for (int x = 0; x < source.w; ++x) {
			const uint32 pixel = palette.color(dest.format, row[x]);
			for (int yy = 0; yy < scale; ++yy)
				for (int xx = 0; xx < scale; ++xx)
					dest.setPixel(x * scale + xx, y * scale + yy, pixel);
		}
	}
}

} // End of namespace Sherlock

#endif

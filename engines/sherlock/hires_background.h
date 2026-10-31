/* ScummVM - Graphic Adventure Engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef SHERLOCK_HIRES_BACKGROUND_H
#define SHERLOCK_HIRES_BACKGROUND_H

#include "graphics/managed_surface.h"

namespace Sherlock {

// The hires background is built from opaque RGB pixels and later map-icon
// painting also writes opaque pixels. Unlike a general alpha-aware blit, a
// same-format copy of that background needs no per-pixel conversion or blend.
inline void copyRoseTattooOpaqueBackground(Graphics::ManagedSurface &destination,
		const Graphics::ManagedSurface &background, int offsetX, int offsetY) {
	destination.clear(0);
	Common::Rect source(offsetX, offsetY, offsetX + destination.w, offsetY + destination.h);
	source.clip(background.getBounds());
	if (source.isEmpty())
		return;
	Common::Rect target = source;
	target.translate(-offsetX, -offsetY);
	if (destination.format != background.format || background.hasTransparentColor()) {
		destination.blitFrom(background, source, target);
		return;
	}
	destination.copyRectToSurface(background.getBasePtr(source.left, source.top), background.pitch,
		target.left, target.top, source.width(), source.height());
}

} // End of namespace Sherlock

#endif

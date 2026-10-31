/* ScummVM - Graphic Adventure Engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef SHERLOCK_HIRES_SPRITE_H
#define SHERLOCK_HIRES_SPRITE_H

#include "graphics/pixelformat.h"

namespace Sherlock {

// Match ManagedSurface::transBlitFrom's 8-bit fixed-point sampling, including
// its truncation at non-integral scales and its source-space horizontal flip.
inline int hiresNativeSample(int destination, int sourceSize, int destinationSize, bool flipped) {
	const int sample = destination * (256 * sourceSize / destinationSize) / 256;
	return flipped ? sourceSize - 1 - sample : sample;
}

// Straight-alpha source-over into an RGBA layer. Premultiplied arithmetic is
// used only for the calculation; the result remains straight-alpha RGBA.
inline uint32 hiresSpriteSourceOver(const Graphics::PixelFormat &format, uint32 source, uint32 destination) {
	byte sa, sr, sg, sb, da, dr, dg, db;
	format.colorToARGB(source, sa, sr, sg, sb);
	if (!sa)
		return destination;
	if (sa == 255)
		return source;
	format.colorToARGB(destination, da, dr, dg, db);
	const uint32 dstWeight = da * (255 - sa);
	const uint32 srcWeight = sa * 255;
	const uint32 weight = srcWeight + dstWeight;
	const byte a = (weight + 127) / 255;
	const byte r = (sr * srcWeight + dr * dstWeight + weight / 2) / weight;
	const byte g = (sg * srcWeight + dg * dstWeight + weight / 2) / weight;
	const byte b = (sb * srcWeight + db * dstWeight + weight / 2) / weight;
	return format.ARGBToColor(a, r, g, b);
}

} // End of namespace Sherlock

#endif

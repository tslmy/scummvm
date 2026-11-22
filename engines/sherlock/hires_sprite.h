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
#include "graphics/surface.h"

namespace Sherlock {

// Match ManagedSurface::transBlitFrom's 8-bit fixed-point sampling, including
// its truncation at non-integral scales and its source-space horizontal flip.
inline int hiresNativeSample(int destination, int sourceSize, int destinationSize, bool flipped) {
	const int sample = destination * (256 * sourceSize / destinationSize) / 256;
	return flipped ? sourceSize - 1 - sample : sample;
}

// Override images use a different nearest-neighbor mapping from native frames.
inline int hiresSpriteNearestSample(int destination, int sourceSize, int destinationSize, bool flipped) {
	const int sample = destination * sourceSize / destinationSize;
	return flipped ? sourceSize - 1 - sample : sample;
}

struct HiresSpriteFilterAxis {
	int first, second;
	uint fraction; // 0..255, in units of 1/256 of a source pixel
};

// Map destination pixel centers to source pixel centers, clamping at the
// image edges. Reflect the destination coordinate for an exact mirror even
// at non-integral scales. Inputs are in the *unclipped* draw rectangle.
inline HiresSpriteFilterAxis hiresSpriteFilterAxis(int destination, int sourceSize, int destinationSize, bool flipped) {
	if (flipped)
		destination = destinationSize - 1 - destination;
	const int64 position = ((2 * (int64)destination + 1) * sourceSize * 256 + destinationSize) /
		(2 * (int64)destinationSize) - 128;
	if (position <= 0)
		return { 0, 0, 0 };
	if (position >= (int64)(sourceSize - 1) * 256)
		return { sourceSize - 1, sourceSize - 1, 0 };
	const int first = position / 256;
	return { first, first + 1, (uint)(position % 256) };
}

// The decoded override is straight-alpha RGBA8888. Interpolate alpha and
// alpha-weighted RGB, then return straight alpha for the existing compositor.
// Hidden RGB of transparent texels cannot contaminate the visible edge.
inline uint32 hiresSpriteBilinearSample(const Graphics::Surface &source,
		const HiresSpriteFilterAxis &x, const HiresSpriteFilterAxis &y) {
	assert(source.format == Graphics::PixelFormat::createFormatRGBA32());
	if (!x.fraction && !y.fraction)
		return source.getPixel(x.first, y.first); // exact, including transparent RGB

	const uint32 pixels[4] = {
		source.getPixel(x.first, y.first), source.getPixel(x.second, y.first),
		source.getPixel(x.first, y.second), source.getPixel(x.second, y.second)
	};
	const uint32 weights[4] = {
		(256 - x.fraction) * (256 - y.fraction), x.fraction * (256 - y.fraction),
		(256 - x.fraction) * y.fraction, x.fraction * y.fraction
	};
	uint32 alpha = 0, red = 0, green = 0, blue = 0;
	for (int i = 0; i < 4; ++i) {
		const uint32 alphaWeight = weights[i] * ((pixels[i] >> source.format.aShift) & 255);
		alpha += alphaWeight;
		red += alphaWeight * ((pixels[i] >> source.format.rShift) & 255);
		green += alphaWeight * ((pixels[i] >> source.format.gShift) & 255);
		blue += alphaWeight * ((pixels[i] >> source.format.bShift) & 255);
	}
	const byte outAlpha = (alpha + 32768) / 65536;
	if (!outAlpha)
		return 0;
	return source.format.ARGBToColor(outAlpha, (red + alpha / 2) / alpha,
		(green + alpha / 2) / alpha, (blue + alpha / 2) / alpha);
}

// Preserve fully opaque interior detail but avoid a hard color switch when
// either alpha fluctuates near 255. Blend toward nearest RGB only when both
// samples are above 223/255 opacity, reaching nearest at 255. A transparent
// or lower-alpha nearest texel never contributes its potentially hidden RGB.
// The output alpha is always the premultiplied bilinear sample's alpha.
inline uint32 hiresSpriteEdgeSample(const Graphics::PixelFormat &format, uint32 nearest, uint32 bilinear) {
	assert(format == Graphics::PixelFormat::createFormatRGBA32());
	const uint32 nearestAlpha = (nearest >> format.aShift) & 255;
	const uint32 bilinearAlpha = (bilinear >> format.aShift) & 255;
	const uint32 opacity = nearestAlpha < bilinearAlpha ? nearestAlpha : bilinearAlpha;
	if (opacity <= 223)
		return bilinear;
	if (opacity == 255)
		return nearest;
	const uint32 weight = opacity - 223; // 1..31 of 32
	const uint32 red = (((bilinear >> format.rShift) & 255) * (32 - weight) +
		((nearest >> format.rShift) & 255) * weight + 16) / 32;
	const uint32 green = (((bilinear >> format.gShift) & 255) * (32 - weight) +
		((nearest >> format.gShift) & 255) * weight + 16) / 32;
	const uint32 blue = (((bilinear >> format.bShift) & 255) * (32 - weight) +
		((nearest >> format.bShift) & 255) * weight + 16) / 32;
	return format.ARGBToColor(bilinearAlpha, red, green, blue);
}

// colorToARGB() without the call for lossless 32-bit formats (hot loops).
inline void hiresUnpack(const Graphics::PixelFormat &format, uint32 color, byte &a, byte &r, byte &g, byte &b) {
	if (format.bytesPerPixel == 4 && !format.rLoss && !format.gLoss && !format.bLoss) {
		a = format.aLoss >= 8 ? 255 : (byte)(color >> format.aShift);
		r = (byte)(color >> format.rShift);
		g = (byte)(color >> format.gShift);
		b = (byte)(color >> format.bShift);
	} else {
		format.colorToARGB(color, a, r, g, b);
	}
}

inline uint32 hiresPack(const Graphics::PixelFormat &format, byte a, byte r, byte g, byte b) {
	if (format.bytesPerPixel == 4 && !format.rLoss && !format.gLoss && !format.bLoss)
		return (format.aLoss >= 8 ? 0 : (uint32)a << format.aShift) | ((uint32)r << format.rShift) |
			((uint32)g << format.gShift) | ((uint32)b << format.bShift);
	return format.ARGBToColor(a, r, g, b);
}

// Straight-alpha source-over into an RGBA layer. Premultiplied arithmetic is
// used only for the calculation; the result remains straight-alpha RGBA.
inline uint32 hiresSpriteSourceOver(const Graphics::PixelFormat &format, uint32 source, uint32 destination) {
	const uint32 sourceAlpha = (source >> format.aShift) & 255;
	if (!sourceAlpha)
		return destination;
	if (sourceAlpha == 255)
		return source;
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

/* ScummVM - Graphic Adventure Engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef SHERLOCK_HIRES_PALETTE_H
#define SHERLOCK_HIRES_PALETTE_H

#include "common/algorithm.h"
#include "graphics/palette.h"
#include "graphics/pixelformat.h"

namespace Sherlock {

// Mirror the palette installed by Screen::setPalette(), which need not be
// the room palette in _cMap (for example, while playing darts).
class RoseTattooHiresPalette {
	byte _colors[Graphics::PALETTE_SIZE];

public:
	RoseTattooHiresPalette() {
		Common::fill(_colors, _colors + Graphics::PALETTE_SIZE, 0);
	}

	void set(const byte *palette, uint start, uint num) {
		assert(start <= Graphics::PALETTE_COUNT && num <= Graphics::PALETTE_COUNT - start);
		Common::copy(palette, palette + num * 3, _colors + start * 3);
	}

	void get(byte *palette, uint start, uint num) const {
		assert(start <= Graphics::PALETTE_COUNT && num <= Graphics::PALETTE_COUNT - start);
		Common::copy(_colors + start * 3, _colors + (start + num) * 3, palette);
	}

	uint32 color(const Graphics::PixelFormat &format, byte index, byte alpha = 255) const {
		if (format.isCLUT8())
			return index;
		return format.ARGBToColor(alpha, _colors[index * 3], _colors[index * 3 + 1], _colors[index * 3 + 2]);
	}
};

} // End of namespace Sherlock

#endif

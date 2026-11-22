/* ScummVM - Graphic Adventure Engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef SHERLOCK_HIRES_WARP_H
#define SHERLOCK_HIRES_WARP_H

#include "common/array.h"
#include "common/str.h"

namespace Sherlock {

// Room warp, sidecar format v1 (shcrt docs/sidecar-format.md, section 4):
// scene_NNN/geometry.json maps the game's logic space (native room pixels)
// to the redrawn picture. Logic (hotspots, walk zones, scripts, saves) never
// changes; the engine draws world layers through the warp and maps the mouse
// back through its inverse.
class HiresRoomWarp {
public:
	bool valid() const { return _valid; }
	void clear();

	// Parse and validate (counts, finite points, no folded cell).
	bool parse(const Common::String &json, int roomWidth, int roomHeight, Common::String &error);

	// Logic point -> picture point, native px (bilinear inside the grid cell).
	void forward(float x, float y, float &px, float &py) const;

	// Build the inverse at an output scale: for every output pixel of the
	// picture (roomWidth*scale x roomHeight*scale) the logic point under its
	// centre, in 1/16 native px; kNoLogic where nothing maps there.
	void buildInverse(int scale);
	int inverseScale() const { return _inverseScale; }
	// Picture point (output px of the room at inverseScale) -> logic point in
	// 1/16 native px; false where no logic point maps there.
	bool inverseAt(int outX, int outY, int &logicX16, int &logicY16) const {
		if (outX < 0 || outY < 0 || outX >= _invW || outY >= _invH)
			return false;
		const uint32 v = _inverse[outY * _invW + outX];
		if (v == kNoLogic)
			return false;
		logicX16 = (int)(v & 0xffff);
		logicY16 = (int)(v >> 16);
		return true;
	}
	// Picture point in native px -> logic point in native px (nearest output
	// pixel of the inverse map). Points that nothing maps to are clamped to
	// the nearest mapped point on the same row (or left unchanged).
	void inverse(int px, int py, int &lx, int &ly) const;

	enum { kNoLogic = 0xffffffff };

private:
	bool _valid = false;
	int _step = 16, _cols = 0, _rows = 0, _roomW = 0, _roomH = 0;
	Common::Array<float> _points; // x, y pairs, row-major
	int _inverseScale = 0, _invW = 0, _invH = 0;
	Common::Array<uint32> _inverse;
};

} // End of namespace Sherlock

#endif

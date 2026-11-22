/* ScummVM - Graphic Adventure Engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef SHERLOCK_HIRES_SIDECAR_H
#define SHERLOCK_HIRES_SIDECAR_H

#include "common/array.h"
#include "common/hashmap.h"
#include "common/hash-str.h"
#include "common/str.h"

namespace Sherlock {

// Override-pack sidecars, format v1 (shcrt docs/sidecar-format.md). A sidecar
// that fails to parse or validate is ignored as a whole: the caller renders as
// if it were absent.

enum {
	kHiresSidecarVersion = 1,
	kHiresSidecarOriginCap = 64,   // native px, each axis
	kHiresSidecarExtentPad = 128   // native px added to the native frame size
};

struct HiresFrameGeometry {
	bool hasEntry = false;     // frames.json has an entry for this frame
	float originX = 0, originY = 0; // override top-left relative to the native frame, native px
	bool ownAlpha = false;     // "alpha": "own"
	bool graded = false;       // neutral master: apply the room grade
};

struct HiresResourceSidecar {
	bool valid = false;
	HiresFrameGeometry defaults;
	Common::HashMap<int, HiresFrameGeometry> frames;

	// Geometry for one frame: its entry merged over defaults, or the defaults
	// with hasEntry false when it has none (or the sidecar is invalid).
	HiresFrameGeometry get(int frameIndex) const;
};

bool parseHiresFramesSidecar(const Common::String &json, HiresResourceSidecar &out, Common::String &error);

// True when an override of assetW x assetH at assetScale fits the engine caps
// for a native frame of nativeW x nativeH.
bool hiresSidecarExtentOk(int assetW, int assetH, int assetScale, int nativeW, int nativeH);

struct HiresGradeEntry {
	bool hasMatrix = false;
	float matrix[3][4] = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 } };
	Common::String lutName;      // relative to scene_NNN/, empty if none
	Common::Array<byte> lut;     // 16*16*16*3 RGB, index ((b*16)+g)*16+r; filled by the caller
	float strength = 1.0f;

	// Grade one straight-alpha colour in place.
	void apply(byte &r, byte &g, byte &b) const;
};

struct HiresRoomGrade {
	bool valid = false;
	bool hasWildcard = false;
	HiresGradeEntry wildcard;
	Common::HashMap<Common::String, HiresGradeEntry, Common::IgnoreCase_Hash, Common::IgnoreCase_EqualTo> byResource;

	const HiresGradeEntry *find(const Common::String &resource) const;
};

bool parseHiresGradeSidecar(const Common::String &json, HiresRoomGrade &out, Common::String &error);

// Convert a decoded 256x16 LUT image (RGB per pixel, row-major) to the
// HiresGradeEntry::lut layout. pixels holds 256*16 entries of 0xRRGGBB.
bool hiresGradeLutFromPixels(const Common::Array<uint32> &pixels, int w, int h, Common::Array<byte> &lut);

} // End of namespace Sherlock

#endif

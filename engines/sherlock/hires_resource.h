/* ScummVM - Graphic Adventure Engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef SHERLOCK_HIRES_RESOURCE_H
#define SHERLOCK_HIRES_RESOURCE_H

#include "common/md5.h"
#include "common/str.h"
#include "common/stream.h"

namespace Sherlock {

// These PC Rose Tattoo names have different image data in VGS.LIB and
// WALK.LIB. The generic resource lookup can select either one during play.
inline bool isAmbiguousRoseTattooHiresResource(const Common::String &name) {
	return name.equalsIgnoreCase("htuprigh_vgs") || name.equalsIgnoreCase("jtdownrg_vgs") ||
		name.equalsIgnoreCase("jtright_vgs") || name.equalsIgnoreCase("jtuprigh_vgs") ||
		name.equalsIgnoreCase("nohat_vgs");
}

// Non-PC Rose Tattoo renderers and unrelated resource keys retain their
// existing lookup behavior. On PC, the bare ambiguous key is never safe.
inline bool canUseRoseTattooHiresKey(const Common::String &name, bool isRoseTattoo, bool is3DO) {
	return !isRoseTattoo || is3DO || !isAmbiguousRoseTattooHiresResource(name);
}

inline Common::String roseTattooHiresSpriteCacheKey(const Common::String &resourceName,
		int frameIndex, int width, int height) {
	return Common::String::format("%s:%d:%dx%d", resourceName.c_str(), frameIndex, width, height);
}

inline bool roseTattooHiresSpriteDimensionsMatch(int width, int height, int expectedWidth, int expectedHeight) {
	return width == expectedWidth && height == expectedHeight;
}

// Hash the complete, already-decompressed ImageFile stream once. This MD5 is
// a non-security content identity; include the parsing mode because the same
// bytes may decode into different frame sequences. Return no key if hashing
// cannot be verified, so callers fall back to the native frame.
inline Common::String roseTattooHiresVariantKey(const Common::String &name,
		Common::SeekableReadStream &stream, bool skipPal, bool animImages) {
#ifdef DISABLE_MD5
	return Common::String();
#else
	const int64 originalPos = stream.pos();
	const int64 size = stream.size();
	if (originalPos < 0 || size < 0 || stream.err())
		return Common::String();
	if (!stream.seek(0)) {
		stream.seek(originalPos);
		return Common::String();
	}
	const Common::String digest = Common::computeStreamMD5AsString(stream);
	const bool complete = !stream.err() && stream.pos() == size && digest.size() == 32;
	const bool restored = stream.seek(originalPos);
	if (!complete || !restored)
		return Common::String();
	return Common::String::format("%s/variants/%s-s%da%d", name.c_str(), digest.c_str(),
		skipPal ? 1 : 0, animImages ? 1 : 0);
#endif
}

} // End of namespace Sherlock

#endif

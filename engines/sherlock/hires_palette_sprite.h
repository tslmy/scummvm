/* ScummVM - Graphic Adventure Engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef SHERLOCK_HIRES_PALETTE_SPRITE_H
#define SHERLOCK_HIRES_PALETTE_SPRITE_H

#include "common/array.h"
#include "common/md5.h"
#include "common/memstream.h"
#include "common/str.h"
#include "graphics/surface.h"
#include "sherlock/hires_palette.h"

namespace Sherlock {

// Select the owner of the actual frame, not merely the person's primary set.
// Keep the original frame index for both the PNG lookup and native fallback.
template <typename Owner, typename Frame>
inline Owner *roseTattooPersonFrameOwner(Owner *mainOwner, Owner *altOwner, const Frame *frame, int &index) {
	index = -1;
	if (!frame)
		return nullptr;
	if (mainOwner && (index = mainOwner->indexOf(frame)) >= 0)
		return mainOwner;
	if (altOwner && (index = altOwner->indexOf(frame)) >= 0)
		return altOwner;
	index = -1;
	return nullptr;
}

// Union the actual decoded pixels of every frame. Person draws use a
// transparent blit with index 255; index 0 is an ordinary opaque color.
class RoseTattooPersonIndices {
public:
	byte bits[32];

	RoseTattooPersonIndices() { Common::fill(bits, bits + sizeof(bits), 0); }

	bool add(const Graphics::Surface &surface) {
		if (!surface.format.isCLUT8() || surface.w <= 0 || surface.h <= 0 || !surface.getPixels())
			return false;
		for (int y = 0; y < surface.h; ++y) {
			const byte *row = static_cast<const byte *>(surface.getBasePtr(0, y));
			for (int x = 0; x < surface.w; ++x) {
				const byte index = row[x];
				if (index != 255)
					bits[index / 8] |= 1 << (index % 8);
			}
		}
		return true;
	}
};

// RT-PAL1 is deliberately independent of scene numbers and unused palette
// entries. The bitset is followed by the installed RGB8 triples, in order.
inline Common::Array<byte> roseTattooPersonPalettePayload(const RoseTattooPersonIndices &indices,
		const RoseTattooHiresPalette &palette) {
	const byte signature[] = { 'R', 'T', '-', 'P', 'A', 'L', '1', 0 };
	Common::Array<byte> payload;
	for (uint i = 0; i < sizeof(signature); ++i)
		payload.push_back(signature[i]);
	for (uint i = 0; i < sizeof(indices.bits); ++i)
		payload.push_back(indices.bits[i]);
	for (uint i = 0; i < 256; ++i) {
		if (indices.bits[i / 8] & (1 << (i % 8))) {
			byte rgb[3];
			palette.get(rgb, i, 1);
			for (uint j = 0; j < 3; ++j)
				payload.push_back(rgb[j]);
		}
	}
	return payload;
}

inline Common::String roseTattooPersonPaletteResource(const Common::String &source,
		const Common::Array<byte> &payload) {
#ifdef DISABLE_MD5
	return Common::String();
#else
	if (source.empty() || payload.empty())
		return Common::String();
	Common::MemoryReadStream stream(payload.begin(), payload.size());
	const Common::String digest = Common::computeStreamMD5AsString(stream);
	if (stream.err() || stream.pos() != (int64)payload.size() || digest.size() != 32)
		return Common::String();
	return Common::String::format("%s/palettes/%s", source.c_str(), digest.c_str());
#endif
}

// Digest/path alone is not sufficient, even in the event of a hash collision.
// A nullptr, short read, extra byte, or any mismatch must be a native fallback.
inline bool roseTattooPersonPaletteMatches(Common::SeekableReadStream *sidecar,
		const Common::Array<byte> &payload) {
	if (!sidecar || sidecar->err() || sidecar->size() != (int64)payload.size() || !sidecar->seek(0))
		return false;
	Common::Array<byte> actual(payload.size());
	if (payload.empty() || sidecar->read(actual.begin(), payload.size()) != payload.size() || sidecar->err())
		return false;
	for (uint i = 0; i < payload.size(); ++i) {
		if (actual[i] != payload[i])
			return false;
	}
	return sidecar->pos() == (int64)payload.size();
}

// Empty means native replay, not an unqualified resource-name lookup.
inline Common::String roseTattooPersonVerifiedResource(const Common::String &candidate,
		Common::SeekableReadStream *sidecar, const Common::Array<byte> &payload) {
	return !candidate.empty() && roseTattooPersonPaletteMatches(sidecar, payload) ? candidate : Common::String();
}

// Keep the full payload in the cache discriminator as well as the digest in
// the pathname. This separates hits and misses even for colliding digests.
inline Common::String roseTattooPersonPaletteCacheDiscriminator(const Common::Array<byte> &payload) {
	static const char hex[] = "0123456789abcdef";
	Common::String result;
	for (uint i = 0; i < payload.size(); ++i) {
		result += hex[payload[i] >> 4];
		result += hex[payload[i] & 15];
	}
	return result;
}

} // End of namespace Sherlock

#endif

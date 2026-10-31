/* ScummVM - Graphic Adventure Engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef SHERLOCK_TATTOO_MASK_TRACE_H
#define SHERLOCK_TATTOO_MASK_TRACE_H

#include "common/algorithm.h"
#include "common/array.h"
#include "common/file.h"
#include "common/rect.h"
#include "graphics/managed_surface.h"
#include "graphics/palette.h"

namespace Sherlock {
namespace Tattoo {

// A diagnostic snapshot of one *actual* drawMaskArea invocation. The pixels
// are world-coordinate CLUT8 pixels from _backBuffer1, before and immediately
// after the mask, not a reconstruction of the later presented frame.
class MaskTraceFrame {
public:
	struct Call {
		uint32 source; // 1 = primary, 2 = secondary, 0 = other
		int32 x, y, width, height;
		Common::Array<byte> runs; // exact value/length bytes read by maskArea
	};

	uint32 serial, epoch;
	int32 scene, mode, width, height, scrollX, scrollY, offsetX, offsetY, viewportWidth;
	int32 conditionFlag, conditionValue; // -1 when no conditional was evaluated
	uint32 outOfBoundsHits, mismatches;
	byte lookup[Graphics::PALETTE_COUNT], palette[Graphics::PALETTE_SIZE];
	Common::Array<byte> before, after;
	Common::Array<uint16> hits;
	Common::Array<Call> calls;

	MaskTraceFrame(uint32 serial_, uint32 epoch_, int scene_, bool mode_,
			const Graphics::ManagedSurface &surface, const Common::Point &scroll,
			const Common::Point &offset, int viewportWidth_, const byte *lookup_, const byte *palette_) :
			serial(serial_), epoch(epoch_), scene(scene_), mode(mode_ ? 1 : 0),
			width(surface.w), height(surface.h), scrollX(scroll.x), scrollY(scroll.y),
			offsetX(offset.x), offsetY(offset.y), viewportWidth(viewportWidth_),
			conditionFlag(-1), conditionValue(-1),
			outOfBoundsHits(0), mismatches(0) {
		assert(surface.format.isCLUT8() && width > 0 && height > 0);
		Common::copy(lookup_, lookup_ + Graphics::PALETTE_COUNT, lookup);
		Common::copy(palette_, palette_ + Graphics::PALETTE_SIZE, palette);
		const uint size = (uint)width * height;
		before.resize(size);
		after.resize(size);
		hits.resize(size, 0);
		copyPixels(surface, before);
	}

	void beginCall(uint32 source, const Common::Point &point, int callWidth, int callHeight) {
		Call call;
		call.source = source;
		call.x = point.x;
		call.y = point.y;
		call.width = callWidth;
		call.height = callHeight;
		calls.push_back(call);
	}

	void recordCondition(int flag, bool value) {
		conditionFlag = flag;
		conditionValue = value ? 1 : 0;
	}

	void recordRun(byte value, byte length) {
		assert(!calls.empty());
		calls.back().runs.push_back(value);
		calls.back().runs.push_back(length);
	}

	// Called alongside the native assignment, even when lookup[index] == index.
	void recordHit(int x, int y) {
		if (x < 0 || y < 0 || x >= width || y >= height) {
			++outOfBoundsHits;
			return;
		}
		uint16 &count = hits[y * width + x];
		if (count == 0xffff)
			++outOfBoundsHits; // Cannot represent the full count; reject the sample.
		else
			++count;
	}

	bool finish(const Graphics::ManagedSurface &surface) {
		if (!surface.format.isCLUT8() || surface.w != width || surface.h != height)
			return false;
		copyPixels(surface, after);
		mismatches = 0;
		for (uint i = 0; i < before.size(); ++i) {
			byte value = before[i];
			for (uint j = 0; j < hits[i]; ++j)
				value = lookup[value];
			if (value != after[i])
				++mismatches;
		}
		return true;
	}

	// Payload of one length-prefixed RTMASK1 record. Integers are little-endian;
	// signed fields use two's-complement 32-bit representation.
	void write(Common::WriteStream &stream) const {
		const int32 fields[] = { (int32)serial, (int32)epoch, scene, mode, width, height,
			scrollX, scrollY, offsetX, offsetY, viewportWidth, conditionFlag, conditionValue, (int32)calls.size(),
			(int32)outOfBoundsHits, (int32)mismatches };
		for (uint i = 0; i < ARRAYSIZE(fields); ++i)
			stream.writeUint32LE((uint32)fields[i]);
		stream.write(lookup, sizeof(lookup));
		stream.write(palette, sizeof(palette));
		stream.write(before.begin(), before.size());
		stream.write(after.begin(), after.size());
		for (uint i = 0; i < hits.size(); ++i)
			stream.writeUint16LE(hits[i]);
		for (uint i = 0; i < calls.size(); ++i) {
			const Call &call = calls[i];
			stream.writeUint32LE(call.source);
			stream.writeUint32LE((uint32)call.x);
			stream.writeUint32LE((uint32)call.y);
			stream.writeUint32LE((uint32)call.width);
			stream.writeUint32LE((uint32)call.height);
			stream.writeUint32LE(call.runs.size());
			if (!call.runs.empty())
				stream.write(call.runs.begin(), call.runs.size());
		}
	}

private:
	void copyPixels(const Graphics::ManagedSurface &surface, Common::Array<byte> &pixels) {
		for (int y = 0; y < height; ++y) {
			const byte *row = (const byte *)surface.getBasePtr(0, y);
			Common::copy(row, row + width, pixels.begin() + y * width);
		}
	}
};

// The writer is inert unless explicitly configured. It samples a bounded
// number of invocations per mode and scene visit, and never affects rendering.
class MaskTraceWriter {
	Common::DumpFile _file;
	uint32 _serial, _epoch, _seen[2], _written[2], _limit, _step;

public:
	MaskTraceWriter();
	bool shouldCapture(bool mode);
	MaskTraceFrame *begin(int scene, bool mode, const Graphics::ManagedSurface &surface,
			const Common::Point &scroll, const Common::Point &offset, int viewportWidth,
			const byte *lookup, const byte *palette);
	void finish(MaskTraceFrame *frame, const Graphics::ManagedSurface &surface);
	void sceneEnded();
};

} // End of namespace Tattoo
} // End of namespace Sherlock

#endif

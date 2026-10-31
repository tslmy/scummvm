/* ScummVM - Graphic Adventure Engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#define FORBIDDEN_SYMBOL_EXCEPTION_getenv

#include "sherlock/tattoo/mask_trace.h"
#include "common/fs.h"
#include "common/memstream.h"
#include "common/textconsole.h"

#include <cstdlib>

namespace Sherlock {
namespace Tattoo {

static uint32 traceSetting(const char *name, uint32 fallback, uint32 maximum) {
	const char *value = getenv(name);
	if (!value || !*value)
		return fallback;
	char *end = nullptr;
	const long parsed = strtol(value, &end, 10);
	if (*end || parsed < 1 || parsed > (long)maximum) {
		warning("Invalid %s; using %u", name, fallback);
		return fallback;
	}
	return (uint32)parsed;
}

MaskTraceWriter::MaskTraceWriter() : _serial(0), _epoch(0), _limit(2), _step(1) {
	_seen[0] = _seen[1] = _written[0] = _written[1] = 0;
	const char *output = getenv("SCUMMVM_SHERLOCK_TATTOO_MASK_TRACE");
	if (!output || !*output)
		return;

	_limit = traceSetting("SCUMMVM_SHERLOCK_TATTOO_MASK_TRACE_LIMIT", 2, 64);
	_step = traceSetting("SCUMMVM_SHERLOCK_TATTOO_MASK_TRACE_STEP", 1, 1000000);
	const Common::Path path(output, Common::Path::kNativeSeparator);
	const Common::FSNode node(path), temporary(path.append(".tmp"));
	// ScummVM's atomic writer uses <destination>.tmp and may truncate it.
	// Refuse either existing path; do not share a destination between runs.
	if (node.exists() || temporary.exists()) {
		warning("Mask trace destination or its .tmp file already exists; not overwriting it");
		return;
	}
	if (!_file.open(node)) {
		warning("Could not open mask trace destination");
		return;
	}
	static const byte magic[] = { 'R', 'T', 'M', 'A', 'S', 'K', '1', 0 };
	if (_file.write(magic, sizeof(magic)) != sizeof(magic) || !_file.flush()) {
		warning("Could not write mask trace header");
		_file.close();
	}
}

bool MaskTraceWriter::shouldCapture(bool mode) {
	const int index = mode ? 1 : 0;
	if (!_file.isOpen() || _written[index] >= _limit)
		return false;
	const uint32 seen = _seen[index]++;
	if (seen % _step != 0)
		return false;
	++_written[index];
	return true;
}

MaskTraceFrame *MaskTraceWriter::begin(int scene, bool mode, const Graphics::ManagedSurface &surface,
		const Common::Point &scroll, const Common::Point &offset, int viewportWidth,
		const byte *lookup, const byte *palette) {
	if (!surface.format.isCLUT8() || surface.w <= 0 || surface.h <= 0 ||
			surface.w > 8192 || surface.h > 8192 || (uint32)surface.w * surface.h > 16777216 ||
			viewportWidth <= 0 || viewportWidth > 8192) {
		warning("Mask trace skipped: unsupported back-buffer dimensions or format");
		return nullptr;
	}
	return new MaskTraceFrame(_serial++, _epoch, scene, mode, surface, scroll, offset,
		viewportWidth, lookup, palette);
}

void MaskTraceWriter::finish(MaskTraceFrame *frame, const Graphics::ManagedSurface &surface) {
	if (!frame)
		return;
	if (!frame->finish(surface)) {
		warning("Mask trace discarded: back buffer changed size or format during mask drawing");
		delete frame;
		return;
	}
	if (frame->outOfBoundsHits || frame->mismatches)
		warning("Mask trace scene %d mode %d: %u out-of-bounds hits, %u replay mismatches",
			frame->scene, frame->mode, frame->outOfBoundsHits, frame->mismatches);

	Common::MemoryWriteStreamDynamic payload(DisposeAfterUse::YES);
	frame->write(payload);
	_file.writeUint32LE((uint32)payload.size());
	const uint32 size = (uint32)payload.size();
	if (_file.write(payload.getData(), size) != size || _file.err() || !_file.flush()) {
		warning("Could not write mask trace record; tracing disabled");
		_file.close();
	}
	delete frame;
}

void MaskTraceWriter::sceneEnded() {
	if (!_file.isOpen())
		return;
	++_epoch;
	_seen[0] = _seen[1] = _written[0] = _written[1] = 0;
}

} // End of namespace Tattoo
} // End of namespace Sherlock

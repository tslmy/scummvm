/* ScummVM - Graphic Adventure Engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "common/util.h"

#include "sherlock/rose_interpolation.h"
#include "sherlock/hires_sprite.h"

namespace Sherlock {

namespace {

// Redraws closer together than this belong to the same step.
const uint32 SAME_STEP_MS = 10;
// After a longer pause (menu, load, script) there is nothing to move from.
const uint32 STALE_STEP_MS = 300;
const uint32 MIN_INTERVAL_MS = 33, MAX_INTERVAL_MS = 200;
// A larger move in one step (native px) is a jump, not a walk.
const float MAX_STEP_DISTANCE = 24.0f;

int roundOffset(float v) {
	return (int)floor(v + 0.5f);
}

uint32 pixelAt(const Graphics::ManagedSurface &s, const Common::Rect &rect, int x, int y) {
	if (!rect.contains(x, y))
		return 0;
	return ((const uint32 *)s.getBasePtr(0, y - rect.top))[x - rect.left];
}

} // End of anonymous namespace

void RoseTattooInterpolator::reset() {
	_previous.clear();
	_current.clear();
	_stepStart = _interval = 0;
}

void RoseTattooInterpolator::beginStep(uint32 now) {
	const uint32 elapsed = now - _stepStart;
	if (_stepStart && elapsed < SAME_STEP_MS) {
		_current.clear();
		return;
	}
	if (_stepStart && elapsed <= STALE_STEP_MS) {
		_previous = _current;
		_interval = CLIP(elapsed, MIN_INTERVAL_MS, MAX_INTERVAL_MS);
	} else {
		_previous.clear();
		_interval = 0;
	}
	_current.clear();
	_stepStart = now;
}

const RoseTattooInterpolator::Sprite *RoseTattooInterpolator::findPrevious(const void *key) const {
	if (!key)
		return nullptr;
	for (uint i = 0; i < _previous.size(); ++i) {
		if (_previous[i].key == key)
			return &_previous[i];
	}
	return nullptr;
}

bool RoseTattooInterpolator::compose(uint32 now, bool lerp, bool crossfade, int scale,
		const Graphics::ManagedSurface &sceneLayer, const Common::Rect &sceneRect,
		Graphics::ManagedSurface &layer, Common::Rect &rect) const {
	if (!_interval || _previous.empty() || now - _stepStart >= _interval)
		return false;
	const float alpha = (float)(now - _stepStart) / _interval;
	const Common::Rect bounds(sceneLayer.w, sceneLayer.h);

	struct Placement {
		const Sprite *cur, *prev; // prev only when cross-fading
		Common::Rect curRect, prevRect;
	};
	Common::Array<Placement> placements;
	Common::Array<Common::Rect> changed;
	for (uint i = 0; i < _current.size(); ++i) {
		const Sprite &cur = _current[i];
		Placement p;
		p.cur = &cur;
		p.prev = nullptr;
		p.curRect = cur.rect;
		bool differs = false;
		const Sprite *prev = findPrevious(cur.key);
		const float dx = prev ? prev->x - cur.x : 0, dy = prev ? prev->y - cur.y : 0;
		if (prev && ABS(dx) <= MAX_STEP_DISTANCE && ABS(dy) <= MAX_STEP_DISTANCE) {
			if (lerp) {
				const int ox = roundOffset(dx * scale * (1 - alpha)), oy = roundOffset(dy * scale * (1 - alpha));
				p.curRect.translate(ox, oy);
				differs = ox || oy;
			}
			if (crossfade && prev->signature != cur.signature) {
				p.prev = prev;
				p.prevRect = prev->rect;
				if (lerp)
					p.prevRect.translate(roundOffset(-dx * scale * alpha), roundOffset(-dy * scale * alpha));
				differs = true;
			}
		}
		placements.push_back(p);

		if (!differs)
			continue;
		Common::Rect area = cur.rect;
		area.extend(p.curRect);
		if (p.prev)
			area.extend(p.prevRect);
		area.clip(bounds);
		if (area.isEmpty())
			continue;
		// Merge overlapping areas so that no pixel is redrawn twice.
		for (uint j = 0; j < changed.size();) {
			if (changed[j].intersects(area)) {
				area.extend(changed[j]);
				changed.remove_at(j);
				j = 0;
			} else {
				++j;
			}
		}
		changed.push_back(area);
	}
	if (changed.empty())
		return false;

	if (layer.empty() || layer.w != sceneLayer.w || layer.h != sceneLayer.h)
		layer.create(sceneLayer.w, sceneLayer.h, sceneLayer.format);
	// Only `rect` is ever read, so only it needs to be brought up to date.
	rect = sceneRect;
	for (uint i = 0; i < changed.size(); ++i) {
		if (rect.isEmpty())
			rect = changed[i];
		else
			rect.extend(changed[i]);
	}
	rect.clip(bounds);
	for (int y = rect.top; y < rect.bottom; ++y)
		memcpy(layer.getBasePtr(rect.left, y), sceneLayer.getBasePtr(rect.left, y), rect.width() * 4);

	const int weight = CLIP((int)(alpha * 256), 0, 256); // of the current frame
	for (uint c = 0; c < changed.size(); ++c) {
		layer.fillRect(changed[c], 0);
		for (uint i = 0; i < placements.size(); ++i) {
			const Placement &p = placements[i];
			Common::Rect area = p.curRect;
			if (p.prev)
				area.extend(p.prevRect);
			area.clip(changed[c]);
			if (area.isEmpty())
				continue;
			const Graphics::ManagedSurface &curPixels = *p.cur->pixels;

			if (!p.prev) {
				for (int y = area.top; y < area.bottom; ++y) {
					const uint32 *src = (const uint32 *)curPixels.getBasePtr(area.left - p.curRect.left,
						y - p.curRect.top);
					uint32 *dst = (uint32 *)layer.getBasePtr(area.left, y);
					for (int x = 0; x < area.width(); ++x)
						dst[x] = hiresSpriteSourceOver(layer.format, src[x], dst[x]);
				}
				continue;
			}

			for (int y = area.top; y < area.bottom; ++y) {
				uint32 *dst = (uint32 *)layer.getBasePtr(area.left, y);
				for (int x = area.left; x < area.right; ++x, ++dst) {
					// Linear blend in premultiplied alpha, so pixels opaque in
					// both frames stay opaque throughout.
					const uint32 cp = pixelAt(curPixels, p.curRect, x, y);
					const uint32 pp = pixelAt(*p.prev->pixels, p.prevRect, x, y);
					uint32 pixel = cp;
					if (cp != pp) {
						byte ca, cr, cg, cb, pa, pr, pg, pb;
						layer.format.colorToARGB(cp, ca, cr, cg, cb);
						layer.format.colorToARGB(pp, pa, pr, pg, pb);
						const uint32 wc = ca * weight, wp = pa * (256 - weight);
						const uint32 sum = wc + wp;
						pixel = !sum ? 0 : layer.format.ARGBToColor((sum + 128) >> 8,
							(cr * wc + pr * wp) / sum, (cg * wc + pg * wp) / sum, (cb * wc + pb * wp) / sum);
					}
					*dst = hiresSpriteSourceOver(layer.format, pixel, *dst);
				}
			}
		}
	}
	return true;
}

} // End of namespace Sherlock

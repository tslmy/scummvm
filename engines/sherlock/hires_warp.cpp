/* ScummVM - Graphic Adventure Engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "sherlock/hires_warp.h"
#include "sherlock/hires_sidecar.h"
#include "common/formats/json.h"
#include "common/util.h"

namespace Sherlock {

namespace {

bool number(const Common::JSONValue *v, double &out) {
	if (!v)
		return false;
	if (v->isIntegerNumber())
		out = (double)v->asIntegerNumber();
	else if (v->isNumber())
		out = v->asNumber();
	else
		return false;
	return out == out && out < 1e7 && out > -1e7;
}

const Common::JSONValue *member(const Common::JSONObject &obj, const char *key) {
	Common::JSONObject::const_iterator it = obj.find(key);
	return it == obj.end() ? nullptr : it->_value;
}

float cross(float ax, float ay, float bx, float by) {
	return ax * by - ay * bx;
}

} // End of anonymous namespace

void HiresRoomWarp::clear() {
	_valid = false;
	_points.clear();
	_inverse.clear();
	_inverseScale = _invW = _invH = 0;
}

bool HiresRoomWarp::parse(const Common::String &json, int roomWidth, int roomHeight, Common::String &error) {
	clear();
	Common::JSONValue *rootValue = Common::JSON::parse(json);
	bool ok = rootValue && rootValue->isObject();
	if (!ok)
		error = "not a JSON object";
	if (ok) {
		const Common::JSONObject &root = rootValue->asObject();
		const Common::JSONValue *version = member(root, "version");
		ok = version && version->isIntegerNumber() && version->asIntegerNumber() >= 1 &&
			version->asIntegerNumber() <= kHiresSidecarVersion;
		if (!ok)
			error = "unsupported or missing version";
		const Common::JSONValue *size = ok ? member(root, "room_size") : nullptr;
		double w = 0, h = 0;
		if (ok && !(size && size->isArray() && size->asArray().size() == 2 &&
				number(size->asArray()[0], w) && number(size->asArray()[1], h) &&
				(int)w == roomWidth && (int)h == roomHeight)) {
			error = "room_size does not match the room";
			ok = false;
		}
		const Common::JSONValue *warp = ok ? member(root, "warp") : nullptr;
		if (ok && !(warp && warp->isObject())) {
			error = "missing warp";
			ok = false;
		}
		if (ok) {
			const Common::JSONObject &wo = warp->asObject();
			double step = 0, cols = 0, rows = 0;
			ok = number(member(wo, "grid_step"), step) && number(member(wo, "cols"), cols) &&
				number(member(wo, "rows"), rows) && step >= 1;
			_step = (int)step;
			_cols = (int)cols;
			_rows = (int)rows;
			ok = ok && _cols == (roomWidth + _step - 1) / _step + 1 && _rows == (roomHeight + _step - 1) / _step + 1;
			if (!ok)
				error = "bad grid size";
			const Common::JSONValue *points = ok ? member(wo, "points") : nullptr;
			ok = ok && points && points->isArray() && (int)points->asArray().size() == _cols * _rows;
			if (ok) {
				_points.resize(_cols * _rows * 2);
				for (int i = 0; ok && i < _cols * _rows; ++i) {
					const Common::JSONValue *p = points->asArray()[i];
					double x, y;
					ok = p->isArray() && p->asArray().size() == 2 && number(p->asArray()[0], x) &&
						number(p->asArray()[1], y);
					_points[i * 2] = (float)x;
					_points[i * 2 + 1] = (float)y;
				}
				if (!ok)
					error = "bad points";
			} else if (error.empty()) {
				error = "bad point count";
			}
		}
	}
	delete rootValue;
	// Fold check: every corner of every cell turns the same way as the identity.
	for (int r = 0; ok && r + 1 < _rows; ++r) {
		for (int c = 0; ok && c + 1 < _cols; ++c) {
			const float *tl = &_points[(r * _cols + c) * 2], *tr = &_points[(r * _cols + c + 1) * 2];
			const float *br = &_points[((r + 1) * _cols + c + 1) * 2], *bl = &_points[((r + 1) * _cols + c) * 2];
			const float *q[4] = { tl, tr, br, bl };
			for (int k = 0; ok && k < 4; ++k) {
				const float *a = q[k], *b = q[(k + 1) % 4], *d = q[(k + 2) % 4];
				if (cross(b[0] - a[0], b[1] - a[1], d[0] - b[0], d[1] - b[1]) <= 0) {
					error = Common::String::format("fold in cell (%d, %d)", c, r);
					ok = false;
				}
			}
		}
	}
	if (!ok) {
		clear();
		return false;
	}
	_roomW = roomWidth;
	_roomH = roomHeight;
	_valid = true;
	return true;
}

void HiresRoomWarp::forward(float x, float y, float &px, float &py) const {
	if (!_valid) {
		px = x;
		py = y;
		return;
	}
	const int c = CLIP((int)(x / _step), 0, _cols - 2), r = CLIP((int)(y / _step), 0, _rows - 2);
	const float u = x / _step - c, v = y / _step - r;
	const float *tl = &_points[(r * _cols + c) * 2], *tr = &_points[(r * _cols + c + 1) * 2];
	const float *bl = &_points[((r + 1) * _cols + c) * 2], *br = &_points[((r + 1) * _cols + c + 1) * 2];
	px = (1 - v) * ((1 - u) * tl[0] + u * tr[0]) + v * ((1 - u) * bl[0] + u * br[0]);
	py = (1 - v) * ((1 - u) * tl[1] + u * tr[1]) + v * ((1 - u) * bl[1] + u * br[1]);
}

void HiresRoomWarp::buildInverse(int scale) {
	_inverse.clear();
	_inverseScale = 0;
	if (!_valid || scale < 1)
		return;
	_invW = _roomW * scale;
	_invH = _roomH * scale;
	_inverse.resize(_invW * _invH, (uint32)kNoLogic);
	for (int r = 0; r + 1 < _rows; ++r) {
		for (int c = 0; c + 1 < _cols; ++c) {
			const float *tl = &_points[(r * _cols + c) * 2], *tr = &_points[(r * _cols + c + 1) * 2];
			const float *bl = &_points[((r + 1) * _cols + c) * 2], *br = &_points[((r + 1) * _cols + c + 1) * 2];
			const float minX = MIN(MIN(tl[0], tr[0]), MIN(bl[0], br[0])), maxX = MAX(MAX(tl[0], tr[0]), MAX(bl[0], br[0]));
			const float minY = MIN(MIN(tl[1], tr[1]), MIN(bl[1], br[1])), maxY = MAX(MAX(tl[1], tr[1]), MAX(bl[1], br[1]));
			const int x0 = MAX(0, (int)floor(minX * scale)), x1 = MIN(_invW - 1, (int)ceil(maxX * scale));
			const int y0 = MAX(0, (int)floor(minY * scale)), y1 = MIN(_invH - 1, (int)ceil(maxY * scale));
			for (int oy = y0; oy <= y1; ++oy) {
				for (int ox = x0; ox <= x1; ++ox) {
					const float px = (ox + 0.5f) / scale, py = (oy + 0.5f) / scale;
					// Newton on the bilinear map from the cell centre.
					float u = 0.5f, v = 0.5f;
					for (int it = 0; it < 12; ++it) {
						const float fx = (1 - v) * ((1 - u) * tl[0] + u * tr[0]) + v * ((1 - u) * bl[0] + u * br[0]) - px;
						const float fy = (1 - v) * ((1 - u) * tl[1] + u * tr[1]) + v * ((1 - u) * bl[1] + u * br[1]) - py;
						const float dxu = (1 - v) * (tr[0] - tl[0]) + v * (br[0] - bl[0]);
						const float dyu = (1 - v) * (tr[1] - tl[1]) + v * (br[1] - bl[1]);
						const float dxv = (1 - u) * (bl[0] - tl[0]) + u * (br[0] - tr[0]);
						const float dyv = (1 - u) * (bl[1] - tl[1]) + u * (br[1] - tr[1]);
						const float det = dxu * dyv - dxv * dyu;
						if (ABS(det) < 1e-6f)
							break;
						u -= (fx * dyv - fy * dxv) / det;
						v -= (fy * dxu - fx * dyu) / det;
						if (ABS(fx) < 1e-3f && ABS(fy) < 1e-3f)
							break;
					}
					if (u < -1e-4f || u > 1.0001f || v < -1e-4f || v > 1.0001f)
						continue;
					const float lx = (c + CLIP(u, 0.0f, 1.0f)) * _step, ly = (r + CLIP(v, 0.0f, 1.0f)) * _step;
					if (lx < 0 || ly < 0 || lx >= _roomW || ly >= _roomH)
						continue;
					_inverse[oy * _invW + ox] = (uint32)(int)(lx * 16) | ((uint32)(int)(ly * 16) << 16);
				}
			}
		}
	}
	_inverseScale = scale;
}

void HiresRoomWarp::inverse(int px, int py, int &lx, int &ly) const {
	lx = px;
	ly = py;
	if (!_valid || !_inverseScale)
		return;
	const int ox = px * _inverseScale + _inverseScale / 2, oy = py * _inverseScale + _inverseScale / 2;
	int x16, y16;
	if (inverseAt(ox, oy, x16, y16)) {
		lx = x16 / 16;
		ly = y16 / 16;
		return;
	}
	// Outside the warped picture: use the nearest mapped pixel on the row.
	for (int d = 1; d < _invW; ++d) {
		if (inverseAt(ox - d, oy, x16, y16) || inverseAt(ox + d, oy, x16, y16)) {
			lx = x16 / 16;
			ly = y16 / 16;
			return;
		}
	}
}

} // End of namespace Sherlock

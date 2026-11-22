/* ScummVM - Graphic Adventure Engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "sherlock/hires_sidecar.h"
#include "common/formats/json.h"
#include "common/util.h"

namespace Sherlock {

namespace {

bool asFinite(const Common::JSONValue *v, double &out) {
	if (!v)
		return false;
	if (v->isIntegerNumber())
		out = (double)v->asIntegerNumber();
	else if (v->isNumber())
		out = v->asNumber();
	else
		return false;
	return out == out && out < 1e30 && out > -1e30; // finite
}

bool asPair(const Common::JSONValue *v, double &x, double &y) {
	if (!v || !v->isArray() || v->asArray().size() != 2)
		return false;
	return asFinite(v->asArray()[0], x) && asFinite(v->asArray()[1], y);
}

const Common::JSONValue *member(const Common::JSONObject &obj, const char *key) {
	Common::JSONObject::const_iterator it = obj.find(key);
	return it == obj.end() ? nullptr : it->_value;
}

bool checkVersion(const Common::JSONObject &root, Common::String &error) {
	const Common::JSONValue *v = member(root, "version");
	if (!v || !v->isIntegerNumber()) {
		error = "missing integer version";
		return false;
	}
	const long long version = v->asIntegerNumber();
	if (version < 1 || version > kHiresSidecarVersion) {
		error = Common::String::format("unsupported version %lld", version);
		return false;
	}
	return true;
}

// Merge the keys present in obj over geo. Unknown keys are ignored.
bool readGeometry(const Common::JSONObject &obj, HiresFrameGeometry &geo, Common::String &error) {
	if (const Common::JSONValue *v = member(obj, "origin")) {
		double x, y;
		if (!asPair(v, x, y) || ABS(x) > kHiresSidecarOriginCap || ABS(y) > kHiresSidecarOriginCap) {
			error = "bad origin";
			return false;
		}
		geo.originX = (float)x;
		geo.originY = (float)y;
	}
	if (const Common::JSONValue *v = member(obj, "alpha")) {
		if (!v->isString() || (v->asString() != "native" && v->asString() != "own")) {
			error = "bad alpha";
			return false;
		}
		geo.ownAlpha = v->asString() == "own";
	}
	if (const Common::JSONValue *v = member(obj, "graded")) {
		if (!v->isBool()) {
			error = "bad graded";
			return false;
		}
		geo.graded = v->asBool();
	}
	if (const Common::JSONValue *v = member(obj, "pivot")) {
		double x, y;
		if (!asPair(v, x, y)) {
			error = "bad pivot";
			return false;
		}
	}
	return true;
}

bool parseIndex(const Common::String &key, int &index) {
	if (key.empty() || key.size() > 6)
		return false;
	index = 0;
	for (uint i = 0; i < key.size(); ++i) {
		if (key[i] < '0' || key[i] > '9')
			return false;
		index = index * 10 + (key[i] - '0');
	}
	return true;
}

byte clampByte(float v) {
	if (v <= 0)
		return 0;
	if (v >= 255)
		return 255;
	return (byte)(v + 0.5f);
}

} // End of anonymous namespace

HiresFrameGeometry HiresResourceSidecar::get(int frameIndex) const {
	if (!valid)
		return HiresFrameGeometry();
	Common::HashMap<int, HiresFrameGeometry>::const_iterator it = frames.find(frameIndex);
	return it == frames.end() ? defaults : it->_value;
}

bool parseHiresFramesSidecar(const Common::String &json, HiresResourceSidecar &out, Common::String &error) {
	out = HiresResourceSidecar();
	Common::JSONValue *rootValue = Common::JSON::parse(json);
	if (!rootValue || !rootValue->isObject()) {
		delete rootValue;
		error = "not a JSON object";
		return false;
	}
	const Common::JSONObject &root = rootValue->asObject();
	bool ok = checkVersion(root, error);
	HiresResourceSidecar result;
	if (ok) {
		if (const Common::JSONValue *d = member(root, "defaults")) {
			ok = d->isObject() && readGeometry(d->asObject(), result.defaults, error);
			if (!d->isObject())
				error = "bad defaults";
		}
	}
	const Common::JSONValue *frames = ok ? member(root, "frames") : nullptr;
	if (ok && (!frames || !frames->isObject())) {
		error = "missing frames object";
		ok = false;
	}
	if (ok) {
		const Common::JSONObject &obj = frames->asObject();
		for (Common::JSONObject::const_iterator it = obj.begin(); ok && it != obj.end(); ++it) {
			int index;
			if (!parseIndex(it->_key, index) || !it->_value->isObject()) {
				error = Common::String::format("bad frame entry '%s'", it->_key.c_str());
				ok = false;
				break;
			}
			HiresFrameGeometry geo = result.defaults;
			ok = readGeometry(it->_value->asObject(), geo, error);
			geo.hasEntry = true;
			result.frames[index] = geo;
		}
	}
	delete rootValue;
	if (!ok)
		return false;
	result.valid = true;
	out = result;
	return true;
}

bool hiresSidecarExtentOk(int assetW, int assetH, int assetScale, int nativeW, int nativeH) {
	if (assetW <= 0 || assetH <= 0 || assetScale <= 0)
		return false;
	return assetW <= (nativeW + kHiresSidecarExtentPad) * assetScale &&
		assetH <= (nativeH + kHiresSidecarExtentPad) * assetScale;
}

void HiresGradeEntry::apply(byte &r, byte &g, byte &b) const {
	const float in[3] = { r / 255.0f, g / 255.0f, b / 255.0f };
	float out[3] = { in[0], in[1], in[2] };
	if (hasMatrix) {
		for (int c = 0; c < 3; ++c) {
			const float v = matrix[c][0] * in[0] + matrix[c][1] * in[1] + matrix[c][2] * in[2] + matrix[c][3];
			out[c] = CLIP(v, 0.0f, 1.0f);
		}
	}
	if (lut.size() == 16 * 16 * 16 * 3) {
		float pos[3];
		int lo[3], hi[3];
		float f[3];
		for (int c = 0; c < 3; ++c) {
			pos[c] = out[c] * 15.0f;
			lo[c] = CLIP((int)pos[c], 0, 15);
			hi[c] = MIN(lo[c] + 1, 15);
			f[c] = pos[c] - lo[c];
		}
		float res[3] = { 0, 0, 0 };
		for (int corner = 0; corner < 8; ++corner) {
			const int ri = (corner & 1) ? hi[0] : lo[0];
			const int gi = (corner & 2) ? hi[1] : lo[1];
			const int bi = (corner & 4) ? hi[2] : lo[2];
			const float w = ((corner & 1) ? f[0] : 1 - f[0]) * ((corner & 2) ? f[1] : 1 - f[1]) *
				((corner & 4) ? f[2] : 1 - f[2]);
			const byte *p = &lut[(((bi * 16) + gi) * 16 + ri) * 3];
			for (int c = 0; c < 3; ++c)
				res[c] += w * p[c] / 255.0f;
		}
		for (int c = 0; c < 3; ++c)
			out[c] = res[c];
	}
	r = clampByte((in[0] + strength * (out[0] - in[0])) * 255.0f);
	g = clampByte((in[1] + strength * (out[1] - in[1])) * 255.0f);
	b = clampByte((in[2] + strength * (out[2] - in[2])) * 255.0f);
}

const HiresGradeEntry *HiresRoomGrade::find(const Common::String &resource) const {
	if (!valid)
		return nullptr;
	Common::HashMap<Common::String, HiresGradeEntry, Common::IgnoreCase_Hash, Common::IgnoreCase_EqualTo>::const_iterator it =
		byResource.find(resource);
	if (it != byResource.end())
		return &it->_value;
	return hasWildcard ? &wildcard : nullptr;
}

bool parseHiresGradeSidecar(const Common::String &json, HiresRoomGrade &out, Common::String &error) {
	out = HiresRoomGrade();
	Common::JSONValue *rootValue = Common::JSON::parse(json);
	if (!rootValue || !rootValue->isObject()) {
		delete rootValue;
		error = "not a JSON object";
		return false;
	}
	const Common::JSONObject &root = rootValue->asObject();
	bool ok = checkVersion(root, error);
	const Common::JSONValue *grades = ok ? member(root, "grades") : nullptr;
	if (ok && (!grades || !grades->isObject() || grades->asObject().empty())) {
		error = "missing grades object";
		ok = false;
	}
	HiresRoomGrade result;
	if (ok) {
		const Common::JSONObject &obj = grades->asObject();
		for (Common::JSONObject::const_iterator it = obj.begin(); ok && it != obj.end(); ++it) {
			if (!it->_value->isObject()) {
				error = "bad grade entry";
				ok = false;
				break;
			}
			const Common::JSONObject &e = it->_value->asObject();
			HiresGradeEntry entry;
			if (const Common::JSONValue *m = member(e, "matrix")) {
				ok = m->isArray() && m->asArray().size() == 3;
				for (int row = 0; ok && row < 3; ++row) {
					const Common::JSONValue *rv = m->asArray()[row];
					ok = rv->isArray() && rv->asArray().size() == 4;
					for (int col = 0; ok && col < 4; ++col) {
						double v;
						ok = asFinite(rv->asArray()[col], v);
						entry.matrix[row][col] = (float)v;
					}
				}
				entry.hasMatrix = ok;
			}
			if (ok) {
				if (const Common::JSONValue *l = member(e, "lut")) {
					ok = l->isString() && !l->asString().empty() && !l->asString().contains('/') &&
						!l->asString().contains('\\') && !l->asString().contains("..");
					if (ok)
						entry.lutName = l->asString();
				}
			}
			if (ok) {
				if (const Common::JSONValue *s = member(e, "strength")) {
					double v;
					ok = asFinite(s, v) && v >= 0 && v <= 1;
					entry.strength = (float)v;
				}
			}
			if (ok && !entry.hasMatrix && entry.lutName.empty())
				ok = false;
			if (!ok) {
				error = Common::String::format("bad grade entry '%s'", it->_key.c_str());
				break;
			}
			if (it->_key == "*") {
				result.wildcard = entry;
				result.hasWildcard = true;
			} else {
				result.byResource[it->_key] = entry;
			}
		}
	}
	delete rootValue;
	if (!ok)
		return false;
	result.valid = true;
	out = result;
	return true;
}

bool hiresGradeLutFromPixels(const Common::Array<uint32> &pixels, int w, int h, Common::Array<byte> &lut) {
	if (w != 256 || h != 16 || pixels.size() != 256 * 16)
		return false;
	lut.resize(16 * 16 * 16 * 3);
	for (int y = 0; y < 16; ++y) {
		for (int x = 0; x < 256; ++x) {
			const uint32 p = pixels[y * 256 + x];
			const int r = x % 16, g = y, b = x / 16;
			byte *dst = &lut[(((b * 16) + g) * 16 + r) * 3];
			dst[0] = (p >> 16) & 0xff;
			dst[1] = (p >> 8) & 0xff;
			dst[2] = p & 0xff;
		}
	}
	return true;
}

} // End of namespace Sherlock

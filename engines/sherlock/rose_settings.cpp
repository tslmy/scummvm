/* ScummVM - Graphic Adventure Engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#define FORBIDDEN_SYMBOL_EXCEPTION_getenv

#include "sherlock/rose_settings.h"
#include "common/config-manager.h"
#include "common/fs.h"
#include "common/debug.h"
#include "common/textconsole.h"

namespace Sherlock {

namespace {

const char *envValue(const char *name) {
	const char *value = getenv(name);
	return value && *value ? value : nullptr;
}

bool hasOption(const char *key) {
	return ConfMan.hasKey(key) && !ConfMan.get(key).empty();
}

} // End of anonymous namespace

int roseTattooHiresScaleSetting() {
	int scale = 1;
	if (const char *env = envValue("SCUMMVM_SHERLOCK_TATTOO_HIRES_SCALE"))
		scale = atoi(env);
	else if (hasOption(ROSE_CONF_HIRES_SCALE))
		scale = ConfMan.getInt(ROSE_CONF_HIRES_SCALE);
	return CLIP(scale, 1, 4);
}

Common::String roseTattooHiresFormatSetting() {
	if (const char *env = envValue("SCUMMVM_SHERLOCK_TATTOO_HIRES_FORMAT"))
		return env;
	if (hasOption(ROSE_CONF_HIRES_FORMAT))
		return ConfMan.get(ROSE_CONF_HIRES_FORMAT);
	// The environment keeps its historical CLUT8 default; a scale chosen in
	// the options means the remastered art, which needs true colour.
	if (!envValue("SCUMMVM_SHERLOCK_TATTOO_HIRES_SCALE") && roseTattooHiresScaleSetting() > 1)
		return "rgba32";
	return "clut8";
}

Common::String roseTattooSpriteFilterSetting() {
	if (const char *env = envValue("SCUMMVM_SHERLOCK_TATTOO_SPRITE_FILTER"))
		return env;
	if (hasOption(ROSE_CONF_SPRITE_FILTER)) {
		switch (ConfMan.getInt(ROSE_CONF_SPRITE_FILTER)) {
		case 1:
			return "bilinear";
		case 2:
			return "edge";
		default:
			return "nearest";
		}
	}
	// Smooth scaling is the better default for the remastered art.
	return envValue("SCUMMVM_SHERLOCK_TATTOO_HIRES_SCALE") ? "nearest" : "edge";
}

const char *roseTattooAssetPackDir() {
	static bool resolved = false;
	static Common::String dir;
	if (!resolved) {
		resolved = true;
		if (const char *env = envValue("SCUMMVM_SHERLOCK_TATTOO_ASSET_OVERRIDES")) {
			dir = env;
		} else if (hasOption(ROSE_CONF_ASSET_PACK)) {
			dir = ConfMan.get(ROSE_CONF_ASSET_PACK);
		} else if (ConfMan.hasKey("path")) {
			const Common::FSNode gameDir(ConfMan.getPath("path"));
			const Common::FSNode pack = gameDir.getChild("remaster");
			if (pack.isDirectory() && pack.getChild("manifest.json").exists())
				dir = pack.getPath().toString('/');
		}
		while (dir.size() > 1 && dir.lastChar() == '/')
			dir.deleteLastChar();
		if (!dir.empty())
			debug(1, "Rose Tattoo asset pack: %s", dir.c_str());
	}
	return dir.empty() ? nullptr : dir.c_str();
}

bool roseTattooPaletteSafeSprites() {
	if (const char *env = envValue("SCUMMVM_SHERLOCK_TATTOO_PALETTE_SAFE_SPRITES"))
		return !strcmp(env, "1");
	return hasOption(ROSE_CONF_PALETTE_SAFE) && ConfMan.getBool(ROSE_CONF_PALETTE_SAFE);
}

RoseFontChoice roseTattooFontChoice() {
	if (const char *env = envValue("SCUMMVM_SHERLOCK_TATTOO_FONT")) {
		if (!strcmp(env, "pack"))
			return kRoseFontPack;
		if (!strcmp(env, "bundled"))
			return kRoseFontBundled;
		if (!strcmp(env, "bitmap"))
			return kRoseFontBitmap;
		return kRoseFontAuto;
	}
	if (hasOption(ROSE_CONF_FONT)) {
		const int value = ConfMan.getInt(ROSE_CONF_FONT);
		if (value >= kRoseFontAuto && value <= kRoseFontBitmap)
			return (RoseFontChoice)value;
	}
	return kRoseFontAuto;
}

Common::String roseTattooFontFile() {
	if (const char *env = envValue("SCUMMVM_SHERLOCK_TATTOO_FONT_FILE"))
		return env;
	return hasOption(ROSE_CONF_FONT_FILE) ? ConfMan.get(ROSE_CONF_FONT_FILE) : Common::String();
}

} // End of namespace Sherlock

/* ScummVM - Graphic Adventure Engine
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef SHERLOCK_ROSE_SETTINGS_H
#define SHERLOCK_ROSE_SETTINGS_H

#include "common/str.h"

namespace Sherlock {

// Rose Tattoo remaster settings. Each player-facing setting is a game option
// (ScummVM config key, set in the game's Engine options tab or scummvm.ini).
// The matching SCUMMVM_SHERLOCK_TATTOO_* environment variable, used by the
// test tooling, overrides the option when it is set and non-empty.
//
// Debug and test hooks (input scripts, frame dumps, post-load scene jumps,
// smoke markers, mask traces, the hires debug views) stay environment-only.

enum RoseFontChoice {
	kRoseFontAuto = 0,    // the pack's font, else the bundled serif, else bitmap
	kRoseFontPack = 1,    // <pack>/fonts/hires_font.ttf only
	kRoseFontBundled = 2, // LiberationSerif-Regular.ttf from ScummVM's fonts.dat
	kRoseFontBitmap = 3   // the original bitmap fonts
};

// Config keys
#define ROSE_CONF_HIRES_SCALE "rt_hires_scale"         // int: 1, 2, 4
#define ROSE_CONF_HIRES_FORMAT "rt_hires_format"       // string: clut8, rgb565, rgba32
#define ROSE_CONF_SPRITE_FILTER "rt_sprite_filter"     // int: 0 nearest, 1 bilinear, 2 edge
#define ROSE_CONF_ASSET_PACK "rt_asset_pack"           // string: pack directory
#define ROSE_CONF_PALETTE_SAFE "rt_palette_safe_sprites" // bool
#define ROSE_CONF_FONT "rt_font"                       // int: RoseFontChoice
#define ROSE_CONF_FONT_FILE "rt_font_file"             // string: a .ttf/.otf file
#define ROSE_CONF_INTERPOLATE "rt_interpolation"       // bool
#define ROSE_CONF_CROSSFADE "rt_crossfade"             // bool

// Output scale (>= 1).
int roseTattooHiresScaleSetting();
// Hires pixel format name: "clut8", "rgb565" or "rgba32" (rgba32 when only the
// game option asks for a scale above 1).
Common::String roseTattooHiresFormatSetting();
// "nearest", "bilinear" or "edge".
Common::String roseTattooSpriteFilterSetting();
// The override pack directory, or nullptr when there is none: the env var,
// else the option, else "<game path>/remaster" if it holds a manifest.json.
// Resolved once per process.
const char *roseTattooAssetPackDir();
bool roseTattooPaletteSafeSprites();
// Smooth movement between animation steps.
bool roseTattooInterpolationSetting();
// Cross-fade from one animation frame to the next.
bool roseTattooCrossfadeSetting();
RoseFontChoice roseTattooFontChoice();
// Explicit font file (env SCUMMVM_SHERLOCK_TATTOO_FONT_FILE or the option);
// empty when not set.
Common::String roseTattooFontFile();

} // End of namespace Sherlock

#endif

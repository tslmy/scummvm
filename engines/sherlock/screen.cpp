/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#define FORBIDDEN_SYMBOL_EXCEPTION_getenv

#include "sherlock/screen.h"
#include "sherlock/hires_background.h"
#include "sherlock/hires_native_reference.h"
#include "sherlock/hires_resource.h"
#include "sherlock/hires_sprite.h"
#include "sherlock/sherlock.h"
#include "sherlock/scalpel/scalpel_screen.h"
#include "sherlock/scalpel/3do/scalpel_3do_screen.h"
#include "sherlock/tattoo/tattoo_screen.h"
#include "common/file.h"
#include "common/fs.h"
#include "common/system.h"
#include "common/util.h"
#include "graphics/pixelformat.h"
#include "graphics/palette.h"
#include "graphics/cursorman.h"
#include "image/png.h"
#ifdef USE_FREETYPE2
#include "graphics/fonts/ttf.h"
#include "graphics/font.h"
#endif

#include <cstdlib>
#include <cstring>

namespace Sherlock {

enum RoseTattooHiresDebugMode {
	kRoseTattooHiresComposite = 0,
	kRoseTattooHiresBackground,
	kRoseTattooHiresMask,
	kRoseTattooHiresNative,
	kRoseTattooHiresNativeReference
};

enum RoseTattooHiresSpriteFilter {
	kRoseTattooHiresSpriteNearest = 0,
	kRoseTattooHiresSpriteBilinear,
	kRoseTattooHiresSpriteEdge
};

Screen *Screen::init(SherlockEngine *vm) {
	if (vm->getGameID() == GType_RoseTattoo)
		return new Tattoo::TattooScreen(vm);
	else if (vm->getPlatform() == Common::kPlatform3DO)
		return new Scalpel::Scalpel3DOScreen(vm);
	else
		return new Scalpel::ScalpelScreen(vm);
}

static int roseTattooHiresScaleFromEnv() {
	const char *scaleEnv = getenv("SCUMMVM_SHERLOCK_TATTOO_HIRES_SCALE");
	if (!scaleEnv || !*scaleEnv)
		return 1;

	int scale = atoi(scaleEnv);
	return scale > 1 ? scale : 1;
}

static int roseTattooHiresDebugModeFromEnv() {
	const char *mode = getenv("SCUMMVM_SHERLOCK_TATTOO_HIRES_DEBUG");
	if (!mode || !*mode || !strcmp(mode, "composite"))
		return kRoseTattooHiresComposite;
	if (!strcmp(mode, "background"))
		return kRoseTattooHiresBackground;
	if (!strcmp(mode, "mask"))
		return kRoseTattooHiresMask;
	if (!strcmp(mode, "native"))
		return kRoseTattooHiresNative;
	if (!strcmp(mode, "native-reference"))
		return kRoseTattooHiresNativeReference;

	warning("Unknown Rose Tattoo hires debug mode '%s'; using composite", mode);
	return kRoseTattooHiresComposite;
}

static Graphics::PixelFormat roseTattooHiresFormatFromEnv() {
	const char *formatEnv = getenv("SCUMMVM_SHERLOCK_TATTOO_HIRES_FORMAT");
	if (!formatEnv || !*formatEnv || !strcmp(formatEnv, "clut8"))
		return Graphics::PixelFormat::createFormatCLUT8();
	if (!strcmp(formatEnv, "rgb565"))
		return Graphics::PixelFormat(2, 5, 6, 5, 0, 11, 5, 0, 0);
	if (!strcmp(formatEnv, "rgba32"))
		return Graphics::PixelFormat::createFormatRGBA32();

	warning("Unknown Rose Tattoo hires format '%s'; using clut8", formatEnv);
	return Graphics::PixelFormat::createFormatCLUT8();
}

static int closestPaletteEntry(const byte *palette, byte r, byte g, byte b) {
	int closest = 0;
	int closestDistance = 256 * 256 * 3;

	for (int idx = 0; idx < 256; ++idx) {
		int dr = (int)r - palette[idx * 3];
		int dg = (int)g - palette[idx * 3 + 1];
		int db = (int)b - palette[idx * 3 + 2];
		int distance = dr * dr + dg * dg + db * db;

		if (distance < closestDistance) {
			closest = idx;
			closestDistance = distance;
		}
	}

	return closest;
}

Screen::Screen(SherlockEngine *vm) : BaseSurface(
			vm->getGameID() == GType_RoseTattoo ? 640 : g_system->getWidth(),
			vm->getGameID() == GType_RoseTattoo ? 480 : g_system->getHeight(),
			vm->getGameID() == GType_RoseTattoo ? Graphics::PixelFormat::createFormatCLUT8() : g_system->getScreenFormat()),
		_vm(vm),
		_backBuffer1(vm->getGameID() == GType_RoseTattoo ? 640 : 320,
			vm->getGameID() == GType_RoseTattoo ? 480 : 200, Graphics::PixelFormat::createFormatCLUT8()),
		_backBuffer2(vm->getGameID() == GType_RoseTattoo ? 640 : 320,
			vm->getGameID() == GType_RoseTattoo ? 480 : 200, Graphics::PixelFormat::createFormatCLUT8()) {
	_transitionSeed = 1;
	_roseTattooHiresScale = vm->getGameID() == GType_RoseTattoo ? roseTattooHiresScaleFromEnv() : 1;
	const char *spriteFilter = getenv("SCUMMVM_SHERLOCK_TATTOO_SPRITE_FILTER");
	_roseTattooHiresSpriteFilter = kRoseTattooHiresSpriteNearest;
	if (vm->getGameID() == GType_RoseTattoo && spriteFilter) {
		if (!strcmp(spriteFilter, "bilinear"))
			_roseTattooHiresSpriteFilter = kRoseTattooHiresSpriteBilinear;
		else if (!strcmp(spriteFilter, "edge"))
			_roseTattooHiresSpriteFilter = kRoseTattooHiresSpriteEdge;
	}
	_roseTattooHiresJournalMode = false;
	_roseTattooHazeEnabled = false;
	_roseTattooHazeFactor[0] = _roseTattooHazeFactor[1] = _roseTattooHazeFactor[2] = 1.0f;
	_roseTattooHiresDebugMode = vm->getGameID() == GType_RoseTattoo ? roseTattooHiresDebugModeFromEnv() : kRoseTattooHiresComposite;
	const char *hazeFallback = vm->getGameID() == GType_RoseTattoo ? getenv("SCUMMVM_SHERLOCK_TATTOO_HAZE_NATIVE_FALLBACK") : nullptr;
	_roseTattooHazeFallback = RoseTattooHazeFallback(hazeFallback && !strcmp(hazeFallback, "1"));
	_roseTattooHiresFormat = vm->getGameID() == GType_RoseTattoo ? roseTattooHiresFormatFromEnv() : Graphics::PixelFormat::createFormatCLUT8();
	_roseTattooHiresSpriteCacheBytes = _roseTattooHiresSpriteCacheClock = 0;
#ifdef USE_FREETYPE2
	_roseTattooHiresFontMissing = false;
#endif
	_fadeStyle = false;
	Common::fill(&_cMap[0], &_cMap[Graphics::PALETTE_SIZE], 0);
	Common::fill(&_sMap[0], &_sMap[Graphics::PALETTE_SIZE], 0);
	Common::fill(&_tMap[0], &_tMap[Graphics::PALETTE_SIZE], 0);

	// Set up the initial font
	setFont(IS_SERRATED_SCALPEL ? 1 : 4);

	// Rose Tattoo specific fields
	_fadeBytesRead = _fadeBytesToRead = 0;
	_oldFadePercent = 0;
	_flushScreen = false;
	_backBuffer.create(_backBuffer1, _backBuffer1.getBounds());
}

Screen::~Screen() {
	Fonts::freeFont();
	clearRoseTattooHiresSpriteCache();
#ifdef USE_FREETYPE2
	for (Common::HashMap<int, Graphics::Font *>::iterator it = _roseTattooHiresFonts.begin();
			it != _roseTattooHiresFonts.end(); ++it)
		delete it->_value;
#endif
}

bool Screen::roseTattooHiresNativeReference() const {
	return _roseTattooHazeFallback.active() ||
		(_roseTattooHiresScale > 1 && _roseTattooHiresDebugMode == kRoseTattooHiresNativeReference);
}

void Screen::selectRoseTattooHazeFallbackScene(int scene) {
	const bool wasNative = roseTattooHiresNativeReference();
	if (!_roseTattooHazeFallback.select(scene) || wasNative == roseTattooHiresNativeReference())
		return;

	// These layers persist independently of the indexed framebuffer. A room
	// policy change must not replay retained UI or scene pixels from the room
	// being left, or bring them back when enhanced rendering resumes.
	clearRoseTattooHiresBackground();
	clearRoseTattooHiresSceneSpriteLayer();
	clearRoseTattooHiresBakedSprites();
	clearRoseTattooHiresSpriteLayer();
#ifdef USE_FREETYPE2
	clearRoseTattooHiresTextLayer();
#endif
}

void Screen::clearRoseTattooHiresBackground() {
	_roseTattooHiresBackground.free();
	_roseTattooHiresComposite.free();
}

void Screen::getPalette(byte palette[Graphics::PALETTE_SIZE]) {
	getPalette(palette, 0, Graphics::PALETTE_COUNT);
}

void Screen::getPalette(byte *palette, uint start, uint num) {
	if (_roseTattooHiresScale > 1 && !_roseTattooHiresFormat.isCLUT8()) {
		_roseTattooPalette.get(palette, start, num);
		return;
	}

	Graphics::Screen::getPalette(palette, start, num);
}

void Screen::setPalette(const byte palette[Graphics::PALETTE_SIZE]) {
	setPalette(palette, 0, Graphics::PALETTE_COUNT);
}

void Screen::setPalette(const byte *palette, uint start, uint num) {
	_roseTattooPalette.set(palette, start, num);

	if (_roseTattooHiresScale > 1 && !_roseTattooHiresFormat.isCLUT8()) {
		// Graphics::Screen::setPalette() forwards to
		// g_system->getPaletteManager()->setPalette(), which some backends
		// (e.g. OpenGLGraphicsManager::setPalette()) assert requires the
		// game screen itself to be palettized/CLUT8 - no longer true once
		// we've switched the backend to RGB565/RGBA32 for the hires
		// composite, so we can't call it here without crashing.
		//
		// However, the mouse cursor (an 8-bit palettized image) still needs
		// a palette to be rendered in color: use CursorMan's dedicated
		// cursor-palette mechanism (backed by OSystem::setCursorPalette(),
		// independent of the game screen's own pixel format) instead of the
		// regular game palette, so the cursor doesn't render solid black.
		CursorMan.replaceCursorPalette(palette, start, num);
		return;
	}

	Graphics::Screen::setPalette(palette, start, num);
}

void Screen::setPalette(const Graphics::Palette &pal, uint start) {
	setPalette(pal.data(), start, pal.size());
}

bool Screen::loadRoseTattooHiresBackgroundFromPath(const Common::Path &overridePath) {
	if (_roseTattooHiresScale <= 1 || roseTattooHiresNativeReference())
		return false;

	// Missing override files are the expected/normal case (not every scene
	// has one), so check existence first rather than letting File::open()
	// fail on its own - it unconditionally logs a "File::open: node does
	// not exist!" warning per missed lookup, which floods the log given how
	// often this path gets probed during ordinary play.
	Common::FSNode overrideNode(overridePath);
	if (!overrideNode.exists())
		return false;

	Common::File file;
	if (!file.open(overrideNode))
		return false;

	Image::PNGDecoder decoder;
	if (!decoder.loadStream(file)) {
		warning("Could not decode Rose Tattoo hires background override: %s",
			overridePath.toString(Common::Path::kNativeSeparator).c_str());
		return false;
	}

	const Graphics::Surface *surface = decoder.getSurface();
	const int expectedWidth = _backBuffer1.width() * _roseTattooHiresScale;
	const int expectedHeight = _backBuffer1.height() * _roseTattooHiresScale;
	if (!surface || surface->w != expectedWidth || surface->h != expectedHeight) {
		warning("Ignoring Rose Tattoo hires background override with unexpected dimensions: %s",
			overridePath.toString(Common::Path::kNativeSeparator).c_str());
		return false;
	}

	_roseTattooHiresBackground.create(expectedWidth, expectedHeight, _roseTattooHiresFormat);
	_roseTattooHiresComposite.create(SHERLOCK_SCREEN_WIDTH * _roseTattooHiresScale,
		SHERLOCK_SCREEN_HEIGHT * _roseTattooHiresScale, _roseTattooHiresFormat);

	const Graphics::Palette &sourcePalette = decoder.getPalette();
	const byte *sourcePaletteData = sourcePalette.data();

	for (int y = 0; y < surface->h; ++y) {
		for (int x = 0; x < surface->w; ++x) {
			uint32 color = surface->getPixel(x, y);
			byte r, g, b;

			if (surface->format.bytesPerPixel == 1 && sourcePaletteData && color < sourcePalette.size()) {
				r = sourcePaletteData[color * 3];
				g = sourcePaletteData[color * 3 + 1];
				b = sourcePaletteData[color * 3 + 2];
			} else {
				surface->format.colorToRGB(color, r, g, b);
			}

			uint32 outputColor = _roseTattooHiresFormat.isCLUT8() ?
				closestPaletteEntry(_cMap, r, g, b) :
				_roseTattooHiresFormat.RGBToColor(r, g, b);
			_roseTattooHiresBackground.setPixel(x, y, outputColor);
		}
	}

	return true;
}

bool Screen::loadRoseTattooHiresBackgroundOverride(int sceneNumber) {
	clearRoseTattooHiresBackground();
	clearRoseTattooHiresSpriteCache();
	loadRoseTattooHiresRoomGrade(sceneNumber);

	if (_roseTattooHiresScale <= 1)
		return false;

	const char *overrideDir = getenv("SCUMMVM_SHERLOCK_TATTOO_ASSET_OVERRIDES");
	if (!overrideDir || !*overrideDir)
		return false;

	Common::Path overridePath(Common::String::format(
		"%s/scene_%03d/background@%dx.png", overrideDir, sceneNumber, _roseTattooHiresScale), '/');
	return loadRoseTattooHiresBackgroundFromPath(overridePath);
}

bool Screen::loadRoseTattooHiresMapOverride() {
	// Unlike normal room scenes, the overhead/travel map isn't loaded through
	// Scene::loadScene(), so TattooMap::show() calls this directly right
	// after sizing _backBuffer1 to the map's own (larger, double-size)
	// canvas. Reuses the same override plumbing as room backgrounds, just
	// with a fixed filename instead of a per-scene one.
	if (_roseTattooHiresScale <= 1)
		return false;

	const char *overrideDir = getenv("SCUMMVM_SHERLOCK_TATTOO_ASSET_OVERRIDES");
	if (!overrideDir || !*overrideDir)
		return false;

	Common::Path overridePath(Common::String::format(
		"%s/sprites/map_vgs/frame_000@%dx.png", overrideDir, _roseTattooHiresScale), '/');
	return loadRoseTattooHiresBackgroundFromPath(overridePath);
}

bool Screen::loadRoseTattooHiresCursorOverride(const Common::String &resourceName, int frameIndex,
		Graphics::Surface &outSurface, int *assetScale) {
	if (_roseTattooHiresScale <= 1 || _roseTattooHiresDebugMode == kRoseTattooHiresNativeReference)
		return false;
	// Never consume a legacy name-only override for the ambiguous PC images,
	// including callers which do not obtain their key from an ImageFile.
	if (!canUseRoseTattooHiresKey(resourceName, IS_ROSE_TATTOO, IS_3DO))
		return false;

	const char *overrideDir = getenv("SCUMMVM_SHERLOCK_TATTOO_ASSET_OVERRIDES");
	if (!overrideDir || !*overrideDir)
		return false;

	// Mixed-scale packs (sidecar format v1): the output scale first; callers
	// that can resample (assetScale given) then accept the largest file below
	// it, else the smallest above it.
	int candidates[8];
	int numCandidates = 0;
	candidates[numCandidates++] = _roseTattooHiresScale;
	if (assetScale) {
		for (int sc = _roseTattooHiresScale - 1; sc >= 1; --sc)
			candidates[numCandidates++] = sc;
		for (int sc = _roseTattooHiresScale + 1; sc <= 4 && numCandidates < 8; ++sc)
			candidates[numCandidates++] = sc;
	}
	Common::Path overridePath;
	Common::FSNode overrideNode;
	int foundScale = 0;
	for (int i = 0; i < numCandidates && !foundScale; ++i) {
		overridePath = Common::Path(Common::String::format(
			"%s/sprites/%s/frame_%03d@%dx.png", overrideDir, resourceName.c_str(), frameIndex,
			candidates[i]), '/');
		// Same rationale as loadRoseTattooHiresBackgroundFromPath() above: most
		// sprite frames don't have an override, so check existence first to
		// avoid File::open()'s "node does not exist!" warning spam.
		overrideNode = Common::FSNode(overridePath);
		if (overrideNode.exists())
			foundScale = candidates[i];
	}
	if (!foundScale)
		return false;
	if (assetScale)
		*assetScale = foundScale;

	Common::File file;
	if (!file.open(overrideNode))
		return false;

	Image::PNGDecoder decoder;
	if (!decoder.loadStream(file)) {
		warning("Could not decode Rose Tattoo hires cursor override: %s",
			overridePath.toString(Common::Path::kNativeSeparator).c_str());
		return false;
	}

	const Graphics::Surface *surface = decoder.getSurface();
	if (!surface || !surface->w || !surface->h)
		return false;

	const Graphics::PixelFormat rgba8888 = Graphics::PixelFormat::createFormatRGBA32();
	outSurface.create(surface->w, surface->h, rgba8888);

	const Graphics::Palette &sourcePalette = decoder.getPalette();
	const byte *sourcePaletteData = sourcePalette.data();
	const bool hasAlpha = decoder.hasTransparentColor() || surface->format.aBits() > 0;

	for (int y = 0; y < surface->h; ++y) {
		for (int x = 0; x < surface->w; ++x) {
			uint32 color = surface->getPixel(x, y);
			byte r, g, b, a = 255;

			if (surface->format.bytesPerPixel == 1 && sourcePaletteData && color < sourcePalette.size()) {
				r = sourcePaletteData[color * 3];
				g = sourcePaletteData[color * 3 + 1];
				b = sourcePaletteData[color * 3 + 2];
				if (decoder.hasTransparentColor() && color == decoder.getTransparentColor())
					a = 0;
			} else {
				surface->format.colorToARGB(color, a, r, g, b);
				if (!hasAlpha)
					a = 255;
			}

			outSurface.setPixel(x, y, rgba8888.ARGBToColor(a, r, g, b));
		}
	}

	return true;
}

void Screen::clearRoseTattooHiresSpriteCache() {
	for (uint i = 0; i < _roseTattooHiresSpriteCache.size(); ++i) {
		Graphics::Surface *surface = _roseTattooHiresSpriteCache[i].surface;
		if (surface) {
			surface->free();
			delete surface;
		}
	}
	_roseTattooHiresSpriteCache.clear();
	_roseTattooHiresSpriteCacheBytes = _roseTattooHiresSpriteCacheClock = 0;
}

const HiresResourceSidecar &Screen::getRoseTattooHiresSidecar(const Common::String &resourceName) {
	Common::HashMap<Common::String, HiresResourceSidecar, Common::IgnoreCase_Hash, Common::IgnoreCase_EqualTo>::const_iterator it =
		_roseTattooHiresSidecars.find(resourceName);
	if (it != _roseTattooHiresSidecars.end())
		return it->_value;

	HiresResourceSidecar sidecar;
	const char *overrideDir = getenv("SCUMMVM_SHERLOCK_TATTOO_ASSET_OVERRIDES");
	if (overrideDir && *overrideDir && !resourceName.empty()) {
		const Common::Path path(Common::String::format("%s/sprites/%s/frames.json", overrideDir,
			resourceName.c_str()), '/');
		const Common::FSNode node(path);
		Common::File file;
		if (node.exists() && file.open(node)) {
			const Common::String json = file.readString(0, file.size());
			Common::String error;
			if (!parseHiresFramesSidecar(json, sidecar, error))
				warning("Ignoring Rose Tattoo sidecar %s: %s", path.toString('/').c_str(), error.c_str());
		}
	}
	_roseTattooHiresSidecars[resourceName] = sidecar;
	return _roseTattooHiresSidecars[resourceName];
}

void Screen::loadRoseTattooHiresRoomGrade(int sceneNumber) {
	_roseTattooHiresRoomGrade = HiresRoomGrade();
	const char *overrideDir = getenv("SCUMMVM_SHERLOCK_TATTOO_ASSET_OVERRIDES");
	if (!overrideDir || !*overrideDir || sceneNumber <= 0)
		return;
	const Common::String sceneDir = Common::String::format("%s/scene_%03d", overrideDir, sceneNumber);
	const Common::Path path(sceneDir + "/grade.json", '/');
	const Common::FSNode node(path);
	Common::File file;
	if (!node.exists() || !file.open(node))
		return;
	const Common::String json = file.readString(0, file.size());
	Common::String error;
	HiresRoomGrade grade;
	if (!parseHiresGradeSidecar(json, grade, error)) {
		warning("Ignoring Rose Tattoo grade %s: %s", path.toString('/').c_str(), error.c_str());
		return;
	}
	// Load the LUT images; a missing or malformed LUT voids the whole sidecar.
	HiresGradeEntry *entries[64];
	int numEntries = 0;
	if (grade.hasWildcard)
		entries[numEntries++] = &grade.wildcard;
	for (Common::HashMap<Common::String, HiresGradeEntry, Common::IgnoreCase_Hash, Common::IgnoreCase_EqualTo>::iterator it =
			grade.byResource.begin(); it != grade.byResource.end() && numEntries < 64; ++it)
		entries[numEntries++] = &it->_value;
	for (int i = 0; i < numEntries; ++i) {
		if (entries[i]->lutName.empty())
			continue;
		const Common::Path lutPath(sceneDir + "/" + entries[i]->lutName, '/');
		const Common::FSNode lutNode(lutPath);
		Common::File lutFile;
		Image::PNGDecoder decoder;
		const Graphics::Surface *surface = nullptr;
		if (lutNode.exists() && lutFile.open(lutNode) && decoder.loadStream(lutFile))
			surface = decoder.getSurface();
		Common::Array<uint32> pixels;
		if (surface && surface->w == 256 && surface->h == 16) {
			const Graphics::Palette &pal = decoder.getPalette();
			pixels.resize(256 * 16);
			for (int y = 0; y < 16; ++y) {
				for (int x = 0; x < 256; ++x) {
					const uint32 c = surface->getPixel(x, y);
					byte r, g, b;
					if (surface->format.bytesPerPixel == 1 && c < pal.size()) {
						r = pal.data()[c * 3];
						g = pal.data()[c * 3 + 1];
						b = pal.data()[c * 3 + 2];
					} else {
						surface->format.colorToRGB(c, r, g, b);
					}
					pixels[y * 256 + x] = (r << 16) | (g << 8) | b;
				}
			}
		}
		if (!hiresGradeLutFromPixels(pixels, surface ? surface->w : 0, surface ? surface->h : 0, entries[i]->lut)) {
			warning("Ignoring Rose Tattoo grade %s: bad LUT %s", path.toString('/').c_str(), entries[i]->lutName.c_str());
			return;
		}
	}
	_roseTattooHiresRoomGrade = grade;
}

const Graphics::Surface *Screen::getRoseTattooHiresSprite(const Common::String &resourceName, int frameIndex,
		int nativeWidth, int nativeHeight, const Common::String &cacheDiscriminator,
		int *assetScaleOut, HiresFrameGeometry *geometryOut) {
	if (resourceName.empty() || frameIndex < 0)
		return nullptr;
	const Common::String key = roseTattooHiresSpriteCacheKey(resourceName, frameIndex, nativeWidth,
		nativeHeight, cacheDiscriminator);
	for (uint i = 0; i < _roseTattooHiresSpriteCache.size(); ++i) {
		HiresSpriteCacheEntry &entry = _roseTattooHiresSpriteCache[i];
		if (entry.key == key) {
			entry.lastUsed = ++_roseTattooHiresSpriteCacheClock;
			if (assetScaleOut)
				*assetScaleOut = entry.assetScale;
			if (geometryOut)
				*geometryOut = entry.geometry;
			return entry.surface;
		}
	}

	HiresFrameGeometry geometry = getRoseTattooHiresSidecar(resourceName).get(frameIndex);
	int assetScale = _roseTattooHiresScale;
	Graphics::Surface *surface = new Graphics::Surface();
	if (!loadRoseTattooHiresCursorOverride(resourceName, frameIndex, *surface, &assetScale)) {
		delete surface;
		surface = nullptr;
	} else if (geometry.hasEntry ? !hiresSidecarExtentOk(surface->w, surface->h, assetScale, nativeWidth, nativeHeight) :
			!roseTattooHiresSpriteDimensionsMatch(surface->w, surface->h, nativeWidth * assetScale, nativeHeight * assetScale)) {
		warning("Ignoring Rose Tattoo hires sprite %s frame %d: unexpected dimensions", resourceName.c_str(), frameIndex);
		surface->free();
		delete surface;
		surface = nullptr;
	}
	if (surface && geometry.graded) {
		// Neutral master: bake the current room's grade (the cache is cleared
		// on every room change, so the key needs no grade discriminator).
		if (const HiresGradeEntry *grade = _roseTattooHiresRoomGrade.find(resourceName)) {
			for (int y = 0; y < surface->h; ++y) {
				for (int x = 0; x < surface->w; ++x) {
					byte a, r, g, b;
					surface->format.colorToARGB(surface->getPixel(x, y), a, r, g, b);
					if (!a)
						continue;
					grade->apply(r, g, b);
					surface->setPixel(x, y, surface->format.ARGBToColor(a, r, g, b));
				}
			}
		}
	}
	if (!surface)
		geometry = HiresFrameGeometry();
	if (assetScaleOut)
		*assetScaleOut = assetScale;
	if (geometryOut)
		*geometryOut = geometry;
	HiresSpriteCacheEntry entry = { key, surface, ++_roseTattooHiresSpriteCacheClock,
		surface ? (uint32)(surface->pitch * surface->h) : 0, assetScale, geometry };
	_roseTattooHiresSpriteCache.push_back(entry);
	_roseTattooHiresSpriteCacheBytes += entry.bytes;

	// Bounded LRU: animated rooms can have hundreds of frames. Keep even a
	// single oversized image, but otherwise cap both decoded bytes and misses.
	const uint32 maxBytes = 32 * 1024 * 1024;
	while (_roseTattooHiresSpriteCache.size() > 1 &&
			(_roseTattooHiresSpriteCache.size() > 256 || _roseTattooHiresSpriteCacheBytes > maxBytes)) {
		uint oldest = 0;
		for (uint i = 1; i < _roseTattooHiresSpriteCache.size(); ++i)
			if (_roseTattooHiresSpriteCache[i].lastUsed < _roseTattooHiresSpriteCache[oldest].lastUsed)
				oldest = i;
		HiresSpriteCacheEntry &victim = _roseTattooHiresSpriteCache[oldest];
		_roseTattooHiresSpriteCacheBytes -= victim.bytes;
		if (victim.surface) {
			victim.surface->free();
			delete victim.surface;
		}
		_roseTattooHiresSpriteCache.remove_at(oldest);
	}
	return surface;
}

Common::Rect Screen::blitRoseTattooHiresSpriteOverride(Graphics::ManagedSurface &layer,
		const Common::String &resourceName, int frameIndex, const ImageFrame &nativeFrame,
		const Common::Point &pt, bool horizFlip, int scaleVal, bool trackNativeProvenance, bool opaqueNative,
		bool worldNativeFallback, const Common::String &cacheDiscriminator) {
	if (_roseTattooHiresScale <= 1 || _roseTattooHiresFormat.isCLUT8() || roseTattooHiresNativeReference())
		return Common::Rect();

	const int scale = _roseTattooHiresScale;
	const int srcW = nativeFrame._frame.w, srcH = nativeFrame._frame.h;
	const int divisor = scaleVal > 0 ? scaleVal : 1;
	const int nativeW = opaqueNative ? srcW : srcW * SCALE_THRESHOLD / divisor;
	const int nativeH = opaqueNative ? srcH : srcH * SCALE_THRESHOLD / divisor;
	if (srcW <= 0 || srcH <= 0 || nativeW <= 0 || nativeH <= 0)
		return Common::Rect();

	int assetScale = scale;
	HiresFrameGeometry geometry;
	const Graphics::Surface *overrideSurface = getRoseTattooHiresSprite(resourceName, frameIndex,
		srcW, srcH, cacheDiscriminator, &assetScale, &geometry);
	const bool hasOverride = overrideSurface != nullptr;
	// UI has its native draw already; the scene needs a replay even for a
	// missing override so that native sprites keep their exact z-order.
	if (!hasOverride && !trackNativeProvenance && !worldNativeFallback)
		return Common::Rect();

	// Quick draws are opaque Surface blits: they do not apply frame offsets or
	// flip. Other draws use the exact destination rectangle of SHtransBlitFrom.
	const Common::Point drawPt = opaqueNative ? pt : Common::Point(
		pt.x + nativeFrame.sDrawXOffset(scaleVal), pt.y + nativeFrame.sDrawYOffset(scaleVal));
	const bool worldLayer = trackNativeProvenance || worldNativeFallback;
	const int outWidth = (worldLayer ? _backBuffer1.w : SHERLOCK_SCREEN_WIDTH) * scale;
	const int outHeight = (worldLayer ? _backBuffer1.h : SHERLOCK_SCREEN_HEIGHT) * scale;
	if (layer.empty() || layer.w != outWidth || layer.h != outHeight) {
		layer.create(outWidth, outHeight, Graphics::PixelFormat::createFormatRGBA32());
		layer.clear(0);
	}

	const int x = drawPt.x * scale;
	const int y = drawPt.y * scale;
	const int width = nativeW * scale, height = nativeH * scale;
	const bool filtered = hasOverride && _roseTattooHiresSpriteFilter != kRoseTattooHiresSpriteNearest &&
		(width != overrideSurface->w || height != overrideSurface->h);
	const bool flipOverride = horizFlip && !opaqueNative;
	const int filterStartX = MAX(0, -x);
	const int filterEndX = MIN(width, layer.w - x);
	Common::Array<HiresSpriteFilterAxis> filterX;
	if (filtered && filterEndX > filterStartX) {
		filterX.resize(filterEndX - filterStartX);
		for (int sx = filterStartX; sx < filterEndX; ++sx)
			filterX[sx - filterStartX] = hiresSpriteFilterAxis(sx, overrideSurface->w, width, flipOverride);
	}
	const uint provenanceSize = _backBuffer1.w * _backBuffer1.h;
	if (trackNativeProvenance && _roseTattooHiresSceneSpriteExpectedValid.size() != provenanceSize) {
		_roseTattooHiresSceneSpriteExpectedNative.resize(provenanceSize, 0);
		_roseTattooHiresSceneSpriteExpectedValid.clear();
		_roseTattooHiresSceneSpriteExpectedValid.resize(provenanceSize, 0);
	}

	if (hasOverride && geometry.hasEntry && !opaqueNative)
		return blitRoseTattooHiresSidecarSprite(layer, *overrideSurface, assetScale, geometry, nativeFrame,
			Common::Rect(x, y, x + width, y + height), nativeW, nativeH, horizFlip, trackNativeProvenance);

	for (int sy = 0; sy < height; ++sy) {
		const int dy = y + sy;
		if (dy < 0 || dy >= layer.h)
			continue;
		const int nativeY = hiresNativeSample(sy / scale, srcH, nativeH, false);
		HiresSpriteFilterAxis filterY = { 0, 0, 0 };
		if (filtered)
			filterY = hiresSpriteFilterAxis(sy, overrideSurface->h, height, false);
		for (int sx = 0; sx < width; ++sx) {
			const int dx = x + sx;
			if (dx < 0 || dx >= layer.w)
				continue;

			const int nativeX = hiresNativeSample(sx / scale, srcW, nativeW, horizFlip && !opaqueNative);
			const byte index = nativeFrame._frame.getPixel(nativeX, nativeY);
			uint32 pixel, nearestPixel = 0;
			if (hasOverride) {
				const int sourceX = hiresSpriteNearestSample(sx, overrideSurface->w, width, flipOverride);
				nearestPixel = overrideSurface->getPixel(sourceX, sy * overrideSurface->h / height);
				pixel = filtered ? hiresSpriteBilinearSample(*overrideSurface, filterX[sx - filterStartX], filterY) : nearestPixel;
				if (filtered && _roseTattooHiresSpriteFilter == kRoseTattooHiresSpriteEdge)
					pixel = hiresSpriteEdgeSample(overrideSurface->format, nearestPixel, pixel);
			} else {
				pixel = _roseTattooPalette.color(layer.format, index,
					opaqueNative || index != TRANSPARENCY ? 255 : 0);
			}

			uint8 a, r, g, b;
			layer.format.colorToARGB(pixel, a, r, g, b);
			// Filtering is visual only: provenance must remain exactly the same
			// as the original nearest-neighbor replay, including override alpha.
			uint8 nearestAlpha = a;
			if (filtered && trackNativeProvenance && !opaqueNative && index == TRANSPARENCY)
				layer.format.colorToARGB(nearestPixel, nearestAlpha, r, g, b);
			if (trackNativeProvenance && (opaqueNative || index != TRANSPARENCY || nearestAlpha)) {
				const int nx = dx / scale, ny = dy / scale;
				const int idx = ny * _backBuffer1.w + nx;
				_roseTattooHiresSceneSpriteExpectedNative[idx] = (byte)_backBuffer1.getPixel(nx, ny);
				_roseTattooHiresSceneSpriteExpectedValid[idx] = 1;
			}
			if (a == 0)
				continue;
			layer.setPixel(dx, dy, hiresSpriteSourceOver(layer.format, pixel, layer.getPixel(dx, dy)));
		}
	}

	return Common::Rect(x, y, x + width, y + height);
}

Common::Rect Screen::blitRoseTattooHiresSidecarSprite(Graphics::ManagedSurface &layer,
		const Graphics::Surface &overrideSurface, int assetScale, const HiresFrameGeometry &geometry,
		const ImageFrame &nativeFrame, const Common::Rect &nativeRect, int nativeW, int nativeH, bool horizFlip,
		bool trackNativeProvenance) {
	// Sidecar format v1: the override sits at `origin` (native px, relative to
	// the native frame's top-left; mirrored on a flipped draw) and is scaled
	// like the native frame (scale factor nativeW / srcW for scaled people).
	const int scale = _roseTattooHiresScale;
	const int srcW = nativeFrame._frame.w, srcH = nativeFrame._frame.h;
	const double kx = (double)nativeW / srcW, ky = (double)nativeH / srcH; // native draw scaling
	const double assetNativeW = (double)overrideSurface.w / assetScale;
	const double assetNativeH = (double)overrideSurface.h / assetScale;
	const double originX = horizFlip ? srcW - (geometry.originX + assetNativeW) : geometry.originX;
	const int ox = nativeRect.left + (int)floor(originX * kx * scale + 0.5);
	const int oy = nativeRect.top + (int)floor(geometry.originY * ky * scale + 0.5);
	const int ow = MAX(1, (int)floor(assetNativeW * kx * scale + 0.5));
	const int oh = MAX(1, (int)floor(assetNativeH * ky * scale + 0.5));
	const Common::Rect overrideRect(ox, oy, ox + ow, oy + oh);

	Common::Rect area = overrideRect;
	area.clip(layer.getBounds());
	if (area.isEmpty())
		return Common::Rect();
	const bool filtered = _roseTattooHiresSpriteFilter != kRoseTattooHiresSpriteNearest &&
		(ow != overrideSurface.w || oh != overrideSurface.h);
	Common::Array<HiresSpriteFilterAxis> filterX;
	if (filtered) {
		filterX.resize(area.width());
		for (int dx = area.left; dx < area.right; ++dx)
			filterX[dx - area.left] = hiresSpriteFilterAxis(dx - ox, overrideSurface.w, ow, horizFlip);
	}

	for (int dy = area.top; dy < area.bottom; ++dy) {
		const int sy = dy - oy;
		HiresSpriteFilterAxis filterY = { 0, 0, 0 };
		if (filtered)
			filterY = hiresSpriteFilterAxis(sy, overrideSurface.h, oh, false);
		for (int dx = area.left; dx < area.right; ++dx) {
			const int sx = dx - ox;
			// Native frame pixel under this output pixel, if any.
			bool insideNative = nativeRect.contains(dx, dy);
			byte index = TRANSPARENCY;
			if (insideNative) {
				const int nativeX = hiresNativeSample((dx - nativeRect.left) / scale, srcW, nativeW, horizFlip);
				const int nativeY = hiresNativeSample((dy - nativeRect.top) / scale, srcH, nativeH, false);
				index = nativeFrame._frame.getPixel(nativeX, nativeY);
			}
			if (!geometry.ownAlpha && index == TRANSPARENCY)
				continue; // "native" alpha: clipped to the native sprite's pixels

			const uint32 nearestPixel = overrideSurface.getPixel(
				hiresSpriteNearestSample(sx, overrideSurface.w, ow, horizFlip), sy * overrideSurface.h / oh);
			uint32 pixel = nearestPixel;
			if (filtered) {
				pixel = hiresSpriteBilinearSample(overrideSurface, filterX[dx - area.left], filterY);
				if (_roseTattooHiresSpriteFilter == kRoseTattooHiresSpriteEdge)
					pixel = hiresSpriteEdgeSample(overrideSurface.format, nearestPixel, pixel);
			}
			uint8 a, r, g, b;
			layer.format.colorToARGB(pixel, a, r, g, b);
			uint8 nearestAlpha, nr, ng, nb;
			layer.format.colorToARGB(nearestPixel, nearestAlpha, nr, ng, nb);
			if (trackNativeProvenance && (index != TRANSPARENCY || nearestAlpha)) {
				// Own-alpha pixels outside the native mask claim whatever the
				// native framebuffer shows there now (normally background), so
				// later native draws over them still win at blend time.
				const int nx = dx / scale, ny = dy / scale;
				if (nx >= 0 && nx < _backBuffer1.w && ny >= 0 && ny < _backBuffer1.h) {
					const int idx = ny * _backBuffer1.w + nx;
					_roseTattooHiresSceneSpriteExpectedNative[idx] = (byte)_backBuffer1.getPixel(nx, ny);
					_roseTattooHiresSceneSpriteExpectedValid[idx] = 1;
				}
			}
			if (!a)
				continue;
			layer.setPixel(dx, dy, hiresSpriteSourceOver(layer.format, pixel, layer.getPixel(dx, dy)));
		}
	}

	Common::Rect result = overrideRect;
	result.extend(nativeRect);
	return result;
}

void Screen::queueRoseTattooHiresSprite(const Common::String &resourceName, int frameIndex,
		const ImageFrame &nativeFrame, const Common::Point &pt, bool horizFlip, int scaleVal) {
	Common::Rect queuedRect = blitRoseTattooHiresSpriteOverride(_roseTattooHiresSpriteLayer,
		resourceName, frameIndex, nativeFrame, pt, horizFlip, scaleVal);
	if (queuedRect.isEmpty())
		return;

	if (_roseTattooHiresSpriteNativeRect.isEmpty())
		_roseTattooHiresSpriteNativeRect = queuedRect;
	else
		_roseTattooHiresSpriteNativeRect.extend(queuedRect);
}

void Screen::queueRoseTattooHiresSceneSprite(const Common::String &resourceName, int frameIndex,
		const ImageFrame &nativeFrame, const Common::Point &pt, bool horizFlip, int scaleVal, bool opaqueNative,
		const Common::String &cacheDiscriminator) {
	// This call is made next to every native scene draw, in draw order. The
	// layer is composited before the remaining native UI diff, so tooltip and
	// menu pixels retain their normal priority.
	Common::Rect queuedRect = blitRoseTattooHiresSpriteOverride(_roseTattooHiresSceneSpriteLayer,
		resourceName, frameIndex, nativeFrame, pt, horizFlip, scaleVal, true, opaqueNative, false, cacheDiscriminator);
	if (!queuedRect.isEmpty()) {
		if (_roseTattooHiresSceneSpriteNativeRect.isEmpty())
			_roseTattooHiresSceneSpriteNativeRect = queuedRect;
		else
			_roseTattooHiresSceneSpriteNativeRect.extend(queuedRect);
	}
}

void Screen::queueRoseTattooHiresPersonSprite(ImageFile *owner, const Common::String &legacyResource,
		int frameIndex, const ImageFrame &nativeFrame, const Common::Point &pt, bool horizFlip, int scaleVal) {
	if (roseTattooHiresNativeReference())
		return;
	const char *mode = getenv("SCUMMVM_SHERLOCK_TATTOO_PALETTE_SAFE_SPRITES");
	if (!IS_ROSE_TATTOO || IS_3DO || !mode || strcmp(mode, "1")) {
		queueRoseTattooHiresSceneSprite(legacyResource, frameIndex, nativeFrame, pt, horizFlip, scaleVal);
		return;
	}

	// A neutral-lit master (frames.json "graded": true) replaces the per-palette
	// variants: the room grade takes the place of the palette projection.
	if (_roseTattooHiresScale > 1 && !_roseTattooHiresFormat.isCLUT8() && frameIndex >= 0 &&
			!legacyResource.empty() && getRoseTattooHiresSidecar(legacyResource).get(frameIndex).graded) {
		queueRoseTattooHiresSceneSprite(legacyResource, frameIndex, nativeFrame, pt, horizFlip, scaleVal);
		return;
	}

	Common::String resource, discriminator;
	const char *overrideDir = getenv("SCUMMVM_SHERLOCK_TATTOO_ASSET_OVERRIDES");
	if (_roseTattooHiresScale > 1 && !_roseTattooHiresFormat.isCLUT8() && owner && frameIndex >= 0 &&
			overrideDir && *overrideDir && owner->indexOf(&nativeFrame) == frameIndex) {
		const Common::String &source = owner->getPersonHiresIdentity();
		const RoseTattooPersonIndices *indices = source.empty() ? nullptr : owner->getPersonOpaqueIndices();
		if (indices) {
			const Common::Array<byte> payload = roseTattooPersonPalettePayload(*indices, _roseTattooPalette);
			const Common::String candidate = roseTattooPersonPaletteResource(source, payload);
			if (!candidate.empty()) {
				const Common::Path path(Common::String::format("%s/sprites/%s/palette.bin",
					overrideDir, candidate.c_str()), '/');
				const Common::FSNode node(path);
				Common::File sidecar;
				if (node.exists() && sidecar.open(node)) {
					resource = roseTattooPersonVerifiedResource(candidate, &sidecar, payload);
				}
				if (!resource.empty())
					discriminator = roseTattooPersonPaletteCacheDiscriminator(payload);
			}
		}
	}
	// An empty resource deliberately invokes the native replay, not the legacy
	// name-only lookup. Validate the sidecar even on decoded-cache hits.
	queueRoseTattooHiresSceneSprite(resource, frameIndex, nativeFrame, pt, horizFlip, scaleVal, false, discriminator);
}

bool Screen::roseTattooSceneOwnsPixel(int screenX, int screenY) const {
	const int x = screenX + _currentScroll.x, y = screenY + _currentScroll.y;
	if (screenX < 0 || screenX >= SHERLOCK_SCREEN_WIDTH || screenY < 0 || screenY >= SHERLOCK_SCREEN_HEIGHT ||
			x < 0 || x >= _backBuffer1.w || y < 0 || y >= _backBuffer1.h ||
			_roseTattooHiresSceneSpriteExpectedValid.size() != (uint)(_backBuffer1.w * _backBuffer1.h))
		return false;
	const int idx = y * _backBuffer1.w + x;
	return _roseTattooHiresSceneSpriteExpectedValid[idx] &&
		getPixel(screenX, screenY) == _roseTattooHiresSceneSpriteExpectedNative[idx];
}

void Screen::setRoseTattooHiresHazeFactors(bool enabled, float r, float g, float b) {
	_roseTattooHazeEnabled = enabled && _roseTattooHiresScale > 1 && !_roseTattooHiresFormat.isCLUT8() &&
		!roseTattooHiresNativeReference();
	_roseTattooHazeFactor[0] = r;
	_roseTattooHazeFactor[1] = g;
	_roseTattooHazeFactor[2] = b;
	if (!_roseTattooHazeEnabled)
		_roseTattooHazeValid.clear();
}

void Screen::beginRoseTattooHiresHaze() {
	if (!_roseTattooHazeEnabled)
		return;
	const uint size = _backBuffer1.w * _backBuffer1.h;
	if (_roseTattooHazeValid.size() != size) {
		_roseTattooHazeValid.clear();
		_roseTattooHazeValid.resize(size, 0);
		_roseTattooHazeExpected.resize(size, 0);
	} else {
		Common::fill(_roseTattooHazeValid.begin(), _roseTattooHazeValid.end(), 0);
	}
}

void Screen::markRoseTattooHiresHaze(int worldX, int worldY, byte before, byte after) {
	if (!_roseTattooHazeEnabled || worldX < 0 || worldY < 0 || worldX >= _backBuffer1.w || worldY >= _backBuffer1.h)
		return;
	const uint idx = worldY * _backBuffer1.w + worldX;
	if (idx >= _roseTattooHazeValid.size())
		return;
	_roseTattooHazeValid[idx] = 1;
	_roseTattooHazeExpected[idx] = after;
	// A replayed scene sprite under the haze keeps ownership of its pixel: the
	// native value changes only because of the haze, which is reapplied below.
	if (idx < _roseTattooHiresSceneSpriteExpectedValid.size() && _roseTattooHiresSceneSpriteExpectedValid[idx] &&
			_roseTattooHiresSceneSpriteExpectedNative[idx] == before)
		_roseTattooHiresSceneSpriteExpectedNative[idx] = after;
}

bool Screen::roseTattooHazePixel(int screenX, int screenY) const {
	const int x = screenX + _currentScroll.x, y = screenY + _currentScroll.y;
	if (!_roseTattooHazeEnabled || screenX < 0 || screenX >= SHERLOCK_SCREEN_WIDTH || screenY < 0 ||
			screenY >= SHERLOCK_SCREEN_HEIGHT || x < 0 || x >= _backBuffer1.w || y < 0 || y >= _backBuffer1.h)
		return false;
	const uint idx = y * _backBuffer1.w + x;
	return idx < _roseTattooHazeValid.size() && _roseTattooHazeValid[idx] &&
		getPixel(screenX, screenY) == _roseTattooHazeExpected[idx];
}

void Screen::applyRoseTattooHiresHaze() {
	if (!_roseTattooHazeEnabled || _roseTattooHazeValid.empty())
		return;
	const int scale = _roseTattooHiresScale;
	// Native coverage of the visible screen, 1 where the haze still shows.
	Common::Array<byte> cover(SHERLOCK_SCREEN_WIDTH * SHERLOCK_SCREEN_HEIGHT, 0);
	bool any = false;
	for (int y = 0; y < SHERLOCK_SCREEN_HEIGHT; ++y)
		for (int x = 0; x < SHERLOCK_SCREEN_WIDTH; ++x)
			if (roseTattooHazePixel(x, y)) {
				cover[y * SHERLOCK_SCREEN_WIDTH + x] = 255;
				any = true;
			}
	if (!any)
		return;
	const int outW = SHERLOCK_SCREEN_WIDTH * scale, outH = SHERLOCK_SCREEN_HEIGHT * scale;
	for (int oy = 0; oy < outH; ++oy) {
		// Bilinear coverage at the output pixel centre, in native pixel centres.
		const HiresSpriteFilterAxis ay = hiresSpriteFilterAxis(oy, SHERLOCK_SCREEN_HEIGHT, outH, false);
		const byte *row0 = &cover[ay.first * SHERLOCK_SCREEN_WIDTH];
		const byte *row1 = &cover[ay.second * SHERLOCK_SCREEN_WIDTH];
		for (int ox = 0; ox < outW; ++ox) {
			const int nx = ox / scale;
			const int ny = oy / scale;
			if (!cover[ny * SHERLOCK_SCREEN_WIDTH + nx] && !row0[nx] && !row1[nx] &&
					!(nx > 0 && (row0[nx - 1] || row1[nx - 1])) &&
					!(nx + 1 < SHERLOCK_SCREEN_WIDTH && (row0[nx + 1] || row1[nx + 1])))
				continue; // fast path: no haze nearby
			const HiresSpriteFilterAxis ax = hiresSpriteFilterAxis(ox, SHERLOCK_SCREEN_WIDTH, outW, false);
			const uint32 w = (row0[ax.first] * (256 - ax.fraction) + row0[ax.second] * ax.fraction) * (256 - ay.fraction) +
				(row1[ax.first] * (256 - ax.fraction) + row1[ax.second] * ax.fraction) * ay.fraction;
			const float k = w / (255.0f * 65536.0f);
			if (k <= 0)
				continue;
			byte a, r, g, b;
			_roseTattooHiresComposite.format.colorToARGB(_roseTattooHiresComposite.getPixel(ox, oy), a, r, g, b);
			const float fr = 1 + k * (_roseTattooHazeFactor[0] - 1);
			const float fg = 1 + k * (_roseTattooHazeFactor[1] - 1);
			const float fb = 1 + k * (_roseTattooHazeFactor[2] - 1);
			_roseTattooHiresComposite.setPixel(ox, oy, _roseTattooHiresComposite.format.ARGBToColor(255,
				(byte)MIN(255.0f, r * fr + 0.5f), (byte)MIN(255.0f, g * fg + 0.5f), (byte)MIN(255.0f, b * fb + 0.5f)));
		}
	}
}

void Screen::blendRoseTattooHiresSceneSpriteLayer() {
	if (_roseTattooHiresSceneSpriteLayer.empty())
		return;

	// The layer and provenance are in room coordinates. Scroll can change
	// after the scene draw, so translate only now, at presentation time.
	Common::Rect spriteRect = _roseTattooHiresSceneSpriteNativeRect;
	spriteRect.clip(_roseTattooHiresSceneSpriteLayer.getBounds());
	const int scale = _roseTattooHiresScale;
	const int scrollX = _currentScroll.x * scale, scrollY = _currentScroll.y * scale;
	spriteRect.clip(Common::Rect(scrollX, scrollY, scrollX + SHERLOCK_SCREEN_WIDTH * scale,
		scrollY + SHERLOCK_SCREEN_HEIGHT * scale));
	for (int y = spriteRect.top; y < spriteRect.bottom; ++y) {
		for (int x = spriteRect.left; x < spriteRect.right; ++x) {
			uint8 a, r, g, b;
			_roseTattooHiresSceneSpriteLayer.format.colorToARGB(
				_roseTattooHiresSceneSpriteLayer.getPixel(x, y), a, r, g, b);
			if (a == 0)
				continue;

			const int screenX = x - scrollX, screenY = y - scrollY;
			if (!roseTattooSceneOwnsPixel(screenX / scale, screenY / scale))
				continue;

			uint8 dstA, dstR, dstG, dstB;
			_roseTattooHiresComposite.format.colorToARGB(_roseTattooHiresComposite.getPixel(screenX, screenY), dstA, dstR, dstG, dstB);

			const uint8 outR = (uint8)((r * a + dstR * (255 - a)) / 255);
			const uint8 outG = (uint8)((g * a + dstG * (255 - a)) / 255);
			const uint8 outB = (uint8)((b * a + dstB * (255 - a)) / 255);
			_roseTattooHiresComposite.setPixel(screenX, screenY,
				_roseTattooHiresComposite.format.ARGBToColor(0xff, outR, outG, outB));
		}
	}
}

void Screen::clearRoseTattooHiresSceneSpriteLayer() {
	if (!_roseTattooHiresSceneSpriteLayer.empty())
		_roseTattooHiresSceneSpriteLayer.fillRect(Common::Rect(0, 0, _roseTattooHiresSceneSpriteLayer.w,
			_roseTattooHiresSceneSpriteLayer.h), 0);

	// Start this frame's occlusion-provenance tracking fresh too - see
	// _roseTattooHiresSceneSpriteExpectedNative's declaration. Only the
	// "valid" flags need clearing; stale expected-native values are
	// harmless since they're never consulted unless their matching valid
	// flag is set.
	if (!_roseTattooHiresSceneSpriteExpectedValid.empty())
		Common::fill(_roseTattooHiresSceneSpriteExpectedValid.begin(), _roseTattooHiresSceneSpriteExpectedValid.end(), 0);
	_roseTattooHiresSceneSpriteNativeRect = Common::Rect();
}

void Screen::bakeRoseTattooHiresSceneSprite(const ImageFile *images, const ImageFrame &frame, const Common::Point &pt) {
	const Common::String resource = images ? images->getHiresResourceName() : Common::String();
	const int index = images ? images->indexOf(&frame) : -1;
	const Common::Rect rect = blitRoseTattooHiresSpriteOverride(_roseTattooHiresBakedSpriteLayer,
		resource, index, frame, pt, false, SCALE_THRESHOLD, false, false, true);
	if (!rect.isEmpty()) {
		if (_roseTattooHiresBakedSpriteRect.isEmpty())
			_roseTattooHiresBakedSpriteRect = rect;
		else
			_roseTattooHiresBakedSpriteRect.extend(rect);
	}
}

void Screen::clearRoseTattooHiresBakedSprites() {
	_roseTattooHiresBakedSpriteLayer.free();
	_roseTattooHiresBakedSpriteRect = Common::Rect();
}

void Screen::blendRoseTattooHiresBakedSprites() {
	if (_roseTattooHiresBakedSpriteLayer.empty())
		return;
	const int scale = _roseTattooHiresScale;
	const int scrollX = _currentScroll.x * scale, scrollY = _currentScroll.y * scale;
	Common::Rect rect = _roseTattooHiresBakedSpriteRect;
	rect.clip(_roseTattooHiresBakedSpriteLayer.getBounds());
	rect.clip(Common::Rect(scrollX, scrollY, scrollX + SHERLOCK_SCREEN_WIDTH * scale,
		scrollY + SHERLOCK_SCREEN_HEIGHT * scale));
	for (int y = rect.top; y < rect.bottom; ++y) {
		for (int x = rect.left; x < rect.right; ++x) {
			byte a, r, g, b, da, dr, dg, db;
			_roseTattooHiresBakedSpriteLayer.format.colorToARGB(_roseTattooHiresBakedSpriteLayer.getPixel(x, y), a, r, g, b);
			if (!a)
				continue;
			const int dx = x - scrollX, dy = y - scrollY;
			_roseTattooHiresComposite.format.colorToARGB(_roseTattooHiresComposite.getPixel(dx, dy), da, dr, dg, db);
			_roseTattooHiresComposite.setPixel(dx, dy, _roseTattooHiresComposite.format.ARGBToColor(255,
				(r * a + dr * (255 - a)) / 255, (g * a + dg * (255 - a)) / 255,
				(b * a + db * (255 - a)) / 255));
		}
	}
}

void Screen::paintRoseTattooHiresWorldSprite(const Common::String &resourceName, int frameIndex,
		const Common::Point &worldPt) {
	// Unlike queueRoseTattooHiresSprite() (which targets the screen-space,
	// scroll-independent _roseTattooHiresSpriteLayer for UI overlays like
	// inventory icons), this paints directly into the persistent, full-world
	// _roseTattooHiresBackground buffer at world/native coordinates - for
	// content like the overhead map's location icons that are drawn once
	// into the map's own oversized scrollable canvas (see
	// TattooMap::drawMapIcons()) rather than redrawn as a screen-space
	// overlay every frame. Baking the icon into the world background this
	// way means the existing scroll-offset sampling in update()/the text
	// and sprite layer blend functions naturally keeps it correctly
	// positioned as the map scrolls, with no extra per-frame bookkeeping.
	if (_roseTattooHiresScale <= 1 || _roseTattooHiresFormat.isCLUT8() || _roseTattooHiresBackground.empty())
		return;

	Graphics::Surface overrideSurface;
	if (!loadRoseTattooHiresCursorOverride(resourceName, frameIndex, overrideSurface))
		return;

	const int scale = _roseTattooHiresScale;
	const int x = worldPt.x * scale;
	const int y = worldPt.y * scale;

	for (int sy = 0; sy < overrideSurface.h; ++sy) {
		const int dy = y + sy;
		if (dy < 0 || dy >= _roseTattooHiresBackground.h)
			continue;
		for (int sx = 0; sx < overrideSurface.w; ++sx) {
			const int dx = x + sx;
			if (dx < 0 || dx >= _roseTattooHiresBackground.w)
				continue;

			uint8 a, r, g, b;
			overrideSurface.format.colorToARGB(overrideSurface.getPixel(sx, sy), a, r, g, b);
			if (a == 0)
				continue;

			if (a == 255) {
				_roseTattooHiresBackground.setPixel(dx, dy,
					_roseTattooHiresBackground.format.ARGBToColor(0xff, r, g, b));
			} else {
				uint8 dstA, dstR, dstG, dstB;
				_roseTattooHiresBackground.format.colorToARGB(_roseTattooHiresBackground.getPixel(dx, dy),
					dstA, dstR, dstG, dstB);
				const uint8 outR = (uint8)((r * a + dstR * (255 - a)) / 255);
				const uint8 outG = (uint8)((g * a + dstG * (255 - a)) / 255);
				const uint8 outB = (uint8)((b * a + dstB * (255 - a)) / 255);
				_roseTattooHiresBackground.setPixel(dx, dy,
					_roseTattooHiresBackground.format.ARGBToColor(0xff, outR, outG, outB));
			}
		}
	}

	overrideSurface.free();
}

void Screen::blendRoseTattooHiresSpriteLayer() {
	if (_roseTattooHiresSpriteLayer.empty())
		return;

	// Unlike blendRoseTattooHiresTextLayer(), we deliberately do NOT repaint
	// the smooth hires *background* image under the dirty rect first: sprite
	// overrides (e.g. inventory item icons) are drawn on top of a UI widget
	// panel, not directly over the live scrolling game view, so repainting
	// the raw scene background there would blow away the panel underneath
	// and let the background bleed through the widget. The native (blocky)
	// icon blit already drawn by the caller as a fallback is a fine base to
	// blend the crisp override on top of.
	Common::Rect spriteRect = _roseTattooHiresSpriteNativeRect;
	spriteRect.clip(_roseTattooHiresSpriteLayer.getBounds());
	for (int y = spriteRect.top; y < spriteRect.bottom; ++y) {
		for (int x = spriteRect.left; x < spriteRect.right; ++x) {
			uint8 a, r, g, b;
			_roseTattooHiresSpriteLayer.format.colorToARGB(_roseTattooHiresSpriteLayer.getPixel(x, y), a, r, g, b);
			if (a == 0)
				continue;

			uint8 dstA, dstR, dstG, dstB;
			_roseTattooHiresComposite.format.colorToARGB(_roseTattooHiresComposite.getPixel(x, y), dstA, dstR, dstG, dstB);

			const uint8 outR = (uint8)((r * a + dstR * (255 - a)) / 255);
			const uint8 outG = (uint8)((g * a + dstG * (255 - a)) / 255);
			const uint8 outB = (uint8)((b * a + dstB * (255 - a)) / 255);
			_roseTattooHiresComposite.setPixel(x, y,
				_roseTattooHiresComposite.format.ARGBToColor(0xff, outR, outG, outB));
		}
	}
}

void Screen::clearRoseTattooHiresSpriteRect(const Common::Rect &nativeRect) {
	if (_roseTattooHiresSpriteLayer.empty())
		return;

	const int scale = _roseTattooHiresScale;
	Common::Rect r(nativeRect.left * scale, nativeRect.top * scale,
		nativeRect.right * scale, nativeRect.bottom * scale);
	r.clip(Common::Rect(0, 0, _roseTattooHiresSpriteLayer.w, _roseTattooHiresSpriteLayer.h));
	if (!r.isEmpty())
		_roseTattooHiresSpriteLayer.fillRect(r, 0);
}

void Screen::clearRoseTattooHiresSpriteLayer() {
	if (!_roseTattooHiresSpriteLayer.empty())
		_roseTattooHiresSpriteLayer.fillRect(Common::Rect(0, 0, _roseTattooHiresSpriteLayer.w,
			_roseTattooHiresSpriteLayer.h), 0);
	_roseTattooHiresSpriteNativeRect = Common::Rect();
}

#ifdef USE_FREETYPE2
Graphics::Font *Screen::getRoseTattooHiresFont(int pixelHeight) {
	if (_roseTattooHiresFontMissing || pixelHeight <= 0)
		return nullptr;

	Common::HashMap<int, Graphics::Font *>::iterator it = _roseTattooHiresFonts.find(pixelHeight);
	if (it != _roseTattooHiresFonts.end())
		return it->_value;

	const char *overrideDir = getenv("SCUMMVM_SHERLOCK_TATTOO_ASSET_OVERRIDES");
	if (!overrideDir || !*overrideDir) {
		_roseTattooHiresFontMissing = true;
		return nullptr;
	}

	Common::Path fontPath(Common::String::format("%s/fonts/hires_font.ttf", overrideDir), '/');
	// Same rationale as the background/sprite override loaders above: avoid
	// File::open()'s "node does not exist!" warning when the font override
	// simply isn't present.
	Common::FSNode fontNode(fontPath);
	if (!fontNode.exists()) {
		_roseTattooHiresFontMissing = true;
		return nullptr;
	}

	Common::File file;
	if (!file.open(fontNode)) {
		_roseTattooHiresFontMissing = true;
		return nullptr;
	}

	// loadTTFFont() takes ownership of the stream (DisposeAfterUse::YES) and
	// reads it fully up front, so a fresh Common::File per requested size is
	// fine - sizes are few (bounded by the handful of distinct FONT*.VGS
	// heights in the game, times _roseTattooHiresScale) and cached below.
	Graphics::Font *font = Graphics::loadTTFFont(file.readStream(file.size()), DisposeAfterUse::YES,
		pixelHeight, Graphics::kTTFSizeModeCharacter, 0, 0, Graphics::kTTFRenderModeNormal);
	if (!font)
		warning("Could not load Rose Tattoo hires text font at size %d: %s",
			pixelHeight, fontPath.toString(Common::Path::kNativeSeparator).c_str());

	_roseTattooHiresFonts[pixelHeight] = font;
	return font;
}

void Screen::queueRoseTattooHiresText(const Common::String &str, const Common::Point &nativePt,
		uint32 rgbColor, int fontHeightPx) {
	// True-color anti-aliased glyphs can't be represented well once
	// quantized down to an 8-bit CLUT8 composite (the whole point is smooth
	// alpha-blended edges) - only bother in the true-color hires modes,
	// same restriction the cursor override already applies. A hires
	// background is optional: the Journal intentionally uses the native
	// nearest-neighbor fallback, and its bitmap glyphs are suppressed by
	// Fonts::writeString() when this TTF is available.
	//
	if (!usesRoseTattooHiresText() || str.empty() ||
			fontHeightPx <= 0)
		return;

	// Match the bitmap font's cell height exactly. kTTFSizeModeCharacter
	// interprets this as the requested character height; the previous 82%
	// heuristic made every hires glyph visibly smaller than the original.
	const int pixelHeight = MAX(1, fontHeightPx * _roseTattooHiresScale);

	Graphics::Font *font = getRoseTattooHiresFont(pixelHeight);
	if (!font)
		return;

	const int outWidth = SHERLOCK_SCREEN_WIDTH * _roseTattooHiresScale;
	const int outHeight = SHERLOCK_SCREEN_HEIGHT * _roseTattooHiresScale;
	if (_roseTattooHiresTextLayer.empty())
		_roseTattooHiresTextLayer.create(outWidth, outHeight, Graphics::PixelFormat::createFormatRGBA32());

	const int x = nativePt.x * _roseTattooHiresScale;
	const int y = nativePt.y * _roseTattooHiresScale;
	const uint32 color = _roseTattooHiresTextLayer.format.ARGBToColor(0xff,
		(rgbColor >> 16) & 0xff, (rgbColor >> 8) & 0xff, rgbColor & 0xff);
	font->drawString(&_roseTattooHiresTextLayer, str, x, y, outWidth - x, color);

	Common::Rect textRect(x, y, x + font->getStringWidth(str), y + font->getFontHeight());
	textRect.clip(Common::Rect(0, 0, outWidth, outHeight));
	if (_roseTattooHiresTextLayerRect.isEmpty())
		_roseTattooHiresTextLayerRect = textRect;
	else
		_roseTattooHiresTextLayerRect.extend(textRect);
}

int Screen::roseTattooHiresStringWidth(const Common::String &str, int fontHeightPx) {
	if (!canUseRoseTattooHiresText(fontHeightPx) || str.empty())
		return -1;

	Graphics::Font *font = getRoseTattooHiresFont(fontHeightPx * _roseTattooHiresScale);
	if (!font)
		return -1;

	// Layout is expressed in the game's native pixels while the TTF is
	// rendered at the output scale. Round up so the final glyph never falls
	// outside the box due to integer division.
	return (font->getStringWidth(str) + _roseTattooHiresScale - 1) /
		_roseTattooHiresScale;
}


void Screen::registerRoseTattooHiresTextRect(const Common::Rect &nativeRect) {
	if (_roseTattooHiresScale <= 1 || nativeRect.isEmpty())
		return;

	const int scale = _roseTattooHiresScale;
	Common::Rect r(nativeRect.left * scale, nativeRect.top * scale,
		nativeRect.right * scale, nativeRect.bottom * scale);
	r.clip(Common::Rect(0, 0, SHERLOCK_SCREEN_WIDTH * scale, SHERLOCK_SCREEN_HEIGHT * scale));
	if (r.isEmpty())
		return;

	if (_roseTattooHiresTextNativeRect.isEmpty())
		_roseTattooHiresTextNativeRect = r;
	else
		_roseTattooHiresTextNativeRect.extend(r);
}

/**
 * Alpha-blends the accumulated hires text layer (see
 * queueRoseTattooHiresText()) on top of the composite that's about to be
 * pushed to the screen. No-op if nothing has been queued (e.g. no hires
 * font override configured, or the format is CLUT8).
 *
 * Deliberately blends pixel-by-pixel via getPixel()/setPixel() rather than
 * ManagedSurface::blendBlitFrom(): that method only works when both
 * surfaces exactly match BlendBlit::getSupportedPixelFormat()'s specific
 * channel byte order, which createFormatRGBA32() does NOT match on
 * little-endian hosts (it silently no-ops with a "only accepts RGBA32!"
 * warning instead of actually blending) - format-agnostic getPixel/
 * setPixel plus colorToARGB()/ARGBToColor() sidesteps that entirely, at
 * the cost of being slower; acceptable since this only runs over the
 * relatively small glyph-covered area of a hires frame that changed.
 *
 * Unlike the composite, this layer is NOT cleared here - see its
 * declaration in screen.h for why it needs to persist across frames on its
 * own instead of being wiped every time it's blended.
 */
void Screen::blendRoseTattooHiresTextLayer() {
	if (_roseTattooHiresTextLayer.empty())
		return;

	// Before blending, repaint the smooth hires background across the
	// whole area covered by this frame's queued text (see
	// _roseTattooHiresTextNativeRect's declaration in screen.h) so none of
	// the blocky upscaled bitmap glyph pixels the composite's normal
	// foreground pass drew there can show through the gaps/edges of the
	// crisp TTF glyphs about to be blended on top. Only possible when a
	// real hires background is loaded (not the raw-upscale-only fallback,
	// where there's nothing smoother to restore from).
	if (!_roseTattooHiresTextNativeRect.isEmpty() && !_roseTattooHiresBackground.empty()) {
		const int bgOffsetX = _currentScroll.x * _roseTattooHiresScale;
		const int bgOffsetY = _currentScroll.y * _roseTattooHiresScale;
		Common::Rect r = _roseTattooHiresTextNativeRect;
		r.clip(Common::Rect(0, 0, _roseTattooHiresComposite.w, _roseTattooHiresComposite.h));
		for (int y = r.top; y < r.bottom; ++y) {
			const int bgY = bgOffsetY + y;
			if (bgY < 0 || bgY >= _roseTattooHiresBackground.h)
				continue;
			for (int x = r.left; x < r.right; ++x) {
				const int bgX = bgOffsetX + x;
				if (bgX < 0 || bgX >= _roseTattooHiresBackground.w)
					continue;
				_roseTattooHiresComposite.setPixel(x, y, _roseTattooHiresBackground.getPixel(bgX, bgY));
			}
		}
	}
	_roseTattooHiresTextNativeRect = Common::Rect();

	Common::Rect textRect = _roseTattooHiresTextLayerRect;
	textRect.clip(Common::Rect(0, 0, _roseTattooHiresTextLayer.w, _roseTattooHiresTextLayer.h));
	for (int y = textRect.top; y < textRect.bottom; ++y) {
		for (int x = textRect.left; x < textRect.right; ++x) {
			uint8 a, r, g, b;
			_roseTattooHiresTextLayer.format.colorToARGB(_roseTattooHiresTextLayer.getPixel(x, y), a, r, g, b);
			if (a == 0)
				continue;

			uint8 dstA, dstR, dstG, dstB;
			_roseTattooHiresComposite.format.colorToARGB(_roseTattooHiresComposite.getPixel(x, y), dstA, dstR, dstG, dstB);

			const uint8 outR = (uint8)((r * a + dstR * (255 - a)) / 255);
			const uint8 outG = (uint8)((g * a + dstG * (255 - a)) / 255);
			const uint8 outB = (uint8)((b * a + dstB * (255 - a)) / 255);
			_roseTattooHiresComposite.setPixel(x, y,
				_roseTattooHiresComposite.format.ARGBToColor(0xff, outR, outG, outB));
		}
	}
}

void Screen::clearRoseTattooHiresTextRect(const Common::Rect &nativeRect) {
	if (_roseTattooHiresTextLayer.empty())
		return;

	const int scale = _roseTattooHiresScale;
	Common::Rect r(nativeRect.left * scale, nativeRect.top * scale,
		nativeRect.right * scale, nativeRect.bottom * scale);
	r.clip(Common::Rect(0, 0, _roseTattooHiresTextLayer.w, _roseTattooHiresTextLayer.h));
	if (!r.isEmpty())
		_roseTattooHiresTextLayer.fillRect(r, 0);
}

void Screen::clearRoseTattooHiresTextLayer() {
	if (!_roseTattooHiresTextLayer.empty())
		_roseTattooHiresTextLayer.fillRect(Common::Rect(0, 0, _roseTattooHiresTextLayer.w,
			_roseTattooHiresTextLayer.h), 0);
	// Scene/map transitions can wipe the persistent text layer between
	// update() calls. Do not carry the old frame's background-repaint region
	// into the next composite either.
	_roseTattooHiresTextNativeRect = Common::Rect();
	_roseTattooHiresTextLayerRect = Common::Rect();
}
#endif

void Screen::presentRoseTattooHiresFrame(int width, int height) {
	// Every native index resolves to black at this exact palette endpoint.
	// Keep composing normally so transient text bookkeeping and retained
	// layers follow their usual lifecycles; blacken only the presented frame.
	// The backend-owned true-color cursor is deliberately left unchanged.
	if (!_roseTattooHiresFormat.isCLUT8() && _roseTattooPalette.isBlack())
		_roseTattooHiresComposite.clear(_roseTattooHiresFormat.RGBToColor(0, 0, 0));

	g_system->copyRectToScreen((const byte *)_roseTattooHiresComposite.getPixels(),
		_roseTattooHiresComposite.pitch, 0, 0, width, height);
	updateScreen();
	_dirtyRects.clear();
}

void Screen::update() {
	if (_roseTattooHiresScale <= 1) {
		Graphics::Screen::update();
		return;
	}

	const int scale = _roseTattooHiresScale;
	const int outWidth = SHERLOCK_SCREEN_WIDTH * scale;
	const int outHeight = SHERLOCK_SCREEN_HEIGHT * scale;

	if (roseTattooHiresNativeReference()) {
		// Do not replay hires background, scene sprites, text, or widget icons:
		// the native framebuffer already contains the mask and UI in draw order.
		// Bitmap text must also remain enabled while producing this frame; some
		// widgets otherwise suppress it in favor of a separate hires text layer.
		if (_roseTattooHiresComposite.empty())
			_roseTattooHiresComposite.create(outWidth, outHeight, _roseTattooHiresFormat);
		roseTattooNativeReferenceFrame(_roseTattooHiresComposite, *this, _roseTattooPalette, scale);
		presentRoseTattooHiresFrame(outWidth, outHeight);
		return;
	}

	if (_roseTattooHiresBackground.empty()) {
		// No hires background override is loaded for the current scene (e.g. the
		// overhead/travel map, which bypasses Scene::loadScene() and therefore
		// never calls loadRoseTattooHiresBackgroundOverride(), or an ordinary
		// room whose override asset is missing). Falling all the way back to
		// Graphics::Screen::update() here would draw only a native-resolution
		// SHERLOCK_SCREEN_WIDTH x SHERLOCK_SCREEN_HEIGHT rectangle into the
		// much larger hires window's top-left corner, leaving the remaining
		// area filled with whatever pixels were already in the OS-level
		// framebuffer (typically stale hires composite data from the last
		// room), which reads as garbled static. Instead, do a simple
		// nearest-neighbor upscale of the native framebuffer so the whole
		// hires window is filled with real, correctly-scaled content.
		if (_roseTattooHiresComposite.empty())
			_roseTattooHiresComposite.create(outWidth, outHeight, _roseTattooHiresFormat);

		const byte *screenPixels = (const byte *)getPixels();
		for (int y = 0; y < SHERLOCK_SCREEN_HEIGHT; ++y) {
			const byte *screenRow = screenPixels + y * pitch;
			for (int x = 0; x < SHERLOCK_SCREEN_WIDTH; ++x) {
				byte nativePixel = screenRow[x];
				const int worldX = x + _currentScroll.x, worldY = y + _currentScroll.y;
				// Remove the native sprite before replaying its replacement. This
				// is essential for transparent edges even without a hires background.
				if (roseTattooSceneOwnsPixel(x, y) && worldX >= 0 && worldX < _backBuffer2.w &&
						worldY >= 0 && worldY < _backBuffer2.h)
					nativePixel = _backBuffer2.getPixel(worldX, worldY);
				const uint32 outputPixel = _roseTattooPalette.color(_roseTattooHiresFormat, nativePixel);
				for (int yy = 0; yy < scale; ++yy) {
					for (int xx = 0; xx < scale; ++xx)
						_roseTattooHiresComposite.setPixel(x * scale + xx, y * scale + yy, outputPixel);
				}
			}
		}

#ifdef USE_FREETYPE2
		blendRoseTattooHiresSceneSpriteLayer();
		blendRoseTattooHiresTextLayer();
#else
		blendRoseTattooHiresSceneSpriteLayer();
#endif
		blendRoseTattooHiresSpriteLayer();
		presentRoseTattooHiresFrame(outWidth, outHeight);
		return;
	}

	const int bgOffsetX = _currentScroll.x * scale;
	const int bgOffsetY = _currentScroll.y * scale;
	const uint32 maskOff = _roseTattooHiresFormat.isCLUT8() ?
		closestPaletteEntry(_cMap, 0, 0, 0) :
		_roseTattooHiresFormat.RGBToColor(0, 0, 0);
	const uint32 maskOn = _roseTattooHiresFormat.isCLUT8() ?
		closestPaletteEntry(_cMap, 255, 0, 255) :
		_roseTattooHiresFormat.RGBToColor(255, 0, 255);

	if (_roseTattooHiresDebugMode == kRoseTattooHiresMask) {
		_roseTattooHiresComposite.clear(maskOff);
	} else if (_roseTattooHiresDebugMode == kRoseTattooHiresNative) {
		_roseTattooHiresComposite.clear(maskOff);
	} else {
		// Both surfaces use the same hires format. Clear the uncovered area
		// once, then copy the visible portion of the scrolled background in a
		// bulk blit instead of doing two nested getPixel/setPixel loops for
		// every output pixel.
		copyRoseTattooOpaqueBackground(_roseTattooHiresComposite, _roseTattooHiresBackground,
			bgOffsetX, bgOffsetY);
		blendRoseTattooHiresBakedSprites();
	}

	if (_roseTattooHiresDebugMode == kRoseTattooHiresBackground) {
#ifdef USE_FREETYPE2
		blendRoseTattooHiresSceneSpriteLayer();
		blendRoseTattooHiresTextLayer();
#else
		blendRoseTattooHiresSceneSpriteLayer();
#endif
		blendRoseTattooHiresSpriteLayer();
		presentRoseTattooHiresFrame(outWidth, outHeight);
		return;
	}

	// The scene layer was queued in the engine's draw order. Replay it now,
	// before the remaining native foreground pixels, so native UI stays on top.
	blendRoseTattooHiresSceneSpriteLayer();
	// The haze pass runs after the scene draws and before the UI.
	applyRoseTattooHiresHaze();

	const byte *screenPixels = (const byte *)getPixels();
	const byte *backgroundPixels = (const byte *)_backBuffer2.getPixels();
	for (int y = 0; y < SHERLOCK_SCREEN_HEIGHT; ++y) {
		const byte *screenRow = screenPixels + y * pitch;
		const int bgNativeY = y + _currentScroll.y;
		if (bgNativeY < 0 || bgNativeY >= _backBuffer2.h)
			continue;

		const byte *bgRow = backgroundPixels + bgNativeY * _backBuffer2.pitch;
		for (int x = 0; x < SHERLOCK_SCREEN_WIDTH; ++x) {
			const int bgNativeX = x + _currentScroll.x;
			if (bgNativeX < 0 || bgNativeX >= _backBuffer2.w)
				continue;

			const byte nativePixel = screenRow[x];
			// Scene draws have already been replayed above. Suppress only the
			// native pixels that still match the last recorded scene draw. A
			// later tooltip/menu/widget changes the final native byte, and is
			// therefore copied after the replay as intended.
			if (roseTattooSceneOwnsPixel(x, y) || roseTattooHazePixel(x, y))
				continue;
			const bool overlayPixel = _roseTattooHiresDebugMode == kRoseTattooHiresNative || nativePixel != bgRow[bgNativeX];
			if (!overlayPixel)
				continue;

			const uint32 outputPixel = _roseTattooHiresDebugMode == kRoseTattooHiresMask ? maskOn :
				_roseTattooPalette.color(_roseTattooHiresFormat, nativePixel);
			for (int yy = 0; yy < scale; ++yy) {
				for (int xx = 0; xx < scale; ++xx)
					_roseTattooHiresComposite.setPixel(x * scale + xx, y * scale + yy, outputPixel);
			}
		}
	}

#ifdef USE_FREETYPE2
	blendRoseTattooHiresTextLayer();
#endif
	blendRoseTattooHiresSpriteLayer();
	presentRoseTattooHiresFrame(outWidth, outHeight);
}

void Screen::activateBackBuffer1() {
	_backBuffer.create(_backBuffer1, _backBuffer1.getBounds());
}

void Screen::activateBackBuffer2() {
	_backBuffer.create(_backBuffer2, _backBuffer2.getBounds());
}

int Screen::equalizePalette(const byte palette[Graphics::PALETTE_SIZE]) {
	int total = 0;
	byte tempPalette[Graphics::PALETTE_SIZE];
	getPalette(tempPalette);

	// For any palette component that doesn't already match the given destination
	// palette, change by 1 towards the reference palette component
	for (int idx = 0; idx < Graphics::PALETTE_SIZE; ++idx) {
		if (tempPalette[idx] > palette[idx]) {
			tempPalette[idx] = MAX((int)palette[idx], (int)tempPalette[idx] - 4);
			++total;
		} else if (tempPalette[idx] < palette[idx]) {
			tempPalette[idx] = MIN((int)palette[idx], (int)tempPalette[idx] + 4);
			++total;
		}
	}

	if (total > 0)
		// Palette changed, so reload it
		setPalette(tempPalette);

	return total;
}

void Screen::fadeToBlack(int speed) {
	byte tempPalette[Graphics::PALETTE_SIZE];
	Common::fill(&tempPalette[0], &tempPalette[Graphics::PALETTE_SIZE], 0);

	while (equalizePalette(tempPalette)) {
		_vm->_events->delay(15 * speed);
	}

	setPalette(tempPalette);
	fillRect(Common::Rect(0, 0, this->w, this->h), 0);
}

void Screen::fadeIn(const byte palette[Graphics::PALETTE_SIZE], int speed) {
	int count = 50;
	while (equalizePalette(palette) && --count) {
		_vm->_events->delay(15 * speed);
	}

	setPalette(palette);
}

void Screen::randomTransition() {
	Events &events = *_vm->_events;
	const int TRANSITION_MULTIPLIER = 0x15a4e35;
	clearDirtyRects();
	assert(IS_SERRATED_SCALPEL);

	for (int idx = 0; idx <= 65535 && !_vm->shouldQuit(); ++idx) {
		_transitionSeed = _transitionSeed * TRANSITION_MULTIPLIER + 1;
		int offset = _transitionSeed & 0xFFFF;

		if (offset < (this->width() * this->height()))
			*((byte *)getPixels() + offset) = *((const byte *)_backBuffer.getPixels() + offset);

		if (idx != 0 && (idx % 300) == 0) {
			// Ensure there's a full screen dirty rect for the next frame update
			if (!isDirty())
				addDirtyRect(Common::Rect(0, 0, this->w, this->h));

			events.pollEvents();
			events.delay(1);
		}
	}

	// Make sure everything has been transferred
	SHblitFrom(_backBuffer);
}

void Screen::verticalTransition() {
	Events &events = *_vm->_events;

	byte table[640];
	Common::fill(&table[0], &table[640], 0);

	for (int yp = 0; yp < this->height(); ++yp) {
		for (int xp = 0; xp < this->width(); ++xp) {
			int temp = (table[xp] >= (this->height() - 3)) ? this->height() - table[xp] :
				_vm->getRandomNumber(3) + 1;

			if (temp) {
				SHblitFrom(_backBuffer1, Common::Point(xp, table[xp]),
					Common::Rect(xp, table[xp], xp + 1, table[xp] + temp));
				table[xp] += temp;
			}
		}

		events.delay(10);
	}
}

void Screen::restoreBackground(const Common::Rect &r) {
	if (r.width() > 0 && r.height() > 0)
		_backBuffer.SHblitFrom(_backBuffer2, Common::Point(r.left, r.top), r);
}

void Screen::slamArea(int16 xp, int16 yp, int16 width_, int16 height_) {
	slamRect(Common::Rect(xp, yp, xp + width_, yp + height_));
}

void Screen::slamRect(const Common::Rect &r) {
	if (r.width() && r.height() > 0) {
		Common::Rect srcRect = r, destRect = r;

		destRect.translate(-_currentScroll.x, -_currentScroll.y);

		if (destRect.left < 0) {
			srcRect.left += -destRect.left;
			destRect.left = 0;
		}
		if (destRect.top < 0) {
			srcRect.top += -destRect.top;
			destRect.top = 0;
		}
		if (destRect.right > SHERLOCK_SCREEN_WIDTH) {
			srcRect.right -= (destRect.left - SHERLOCK_SCREEN_WIDTH);
			destRect.right = SHERLOCK_SCREEN_WIDTH;
		}
		if (destRect.bottom > SHERLOCK_SCREEN_HEIGHT) {
			srcRect.bottom -= (destRect.bottom - SHERLOCK_SCREEN_HEIGHT);
			destRect.bottom = SHERLOCK_SCREEN_HEIGHT;
		}

		if (srcRect.isValidRect())
			SHblitFrom(_backBuffer, Common::Point(destRect.left, destRect.top), srcRect);
	}
}

void Screen::flushImage(ImageFrame *frame, const Common::Point &pt, int16 *xp, int16 *yp,
		int16 *width_, int16 *height_) {
	Common::Point imgPos = pt + frame->_offset;
	Common::Rect newBounds(imgPos.x, imgPos.y, imgPos.x + frame->_frame.w, imgPos.y + frame->_frame.h);
	Common::Rect oldBounds(*xp, *yp, *xp + *width_, *yp + *height_);

	if (!_flushScreen) {
		// See if the areas of the old and new overlap, and if so combine the areas
		if (newBounds.intersects(oldBounds)) {
			Common::Rect mergedBounds = newBounds;
			mergedBounds.extend(oldBounds);
			mergedBounds.right += 1;
			mergedBounds.bottom += 1;

			slamRect(mergedBounds);
		} else {
			// The two areas are independent, so copy them both
			slamRect(newBounds);
			slamRect(oldBounds);
		}
	}

	*xp = newBounds.left;
	*yp = newBounds.top;
	*width_ = newBounds.width();
	*height_ = newBounds.height();
}

void Screen::flushScaleImage(ImageFrame *frame, const Common::Point &pt, int16 *xp, int16 *yp,
		int16 *width_, int16 *height_, int scaleVal) {
	Common::Point imgPos(pt.x + frame->sDrawXOffset(scaleVal), pt.y + frame->sDrawYOffset(scaleVal));
	Common::Rect newBounds(imgPos.x, imgPos.y, imgPos.x + frame->sDrawXSize(scaleVal),
		imgPos.y + frame->sDrawYSize(scaleVal));
	Common::Rect oldBounds(*xp, *yp, *xp + *width_, *yp + *height_);

	if (!_flushScreen) {
		// See if the areas of the old and new overlap, and if so combine the areas
		if (newBounds.intersects(oldBounds)) {
			Common::Rect mergedBounds = newBounds;
			mergedBounds.extend(oldBounds);
			mergedBounds.right += 1;
			mergedBounds.bottom += 1;

			slamRect(mergedBounds);
		} else {
			// The two areas are independent, so copy them both
			slamRect(newBounds);
			slamRect(oldBounds);
		}
	}

	*xp = newBounds.left;
	*yp = newBounds.top;
	*width_ = newBounds.width();
	*height_ = newBounds.height();
}

void Screen::flushImage(ImageFrame *frame, const Common::Point &pt, Common::Rect &newBounds, int scaleVal) {
	Common::Point newPos(newBounds.left, newBounds.top);
	Common::Point newSize(newBounds.width(), newBounds.height());

	if (scaleVal == SCALE_THRESHOLD)
		flushImage(frame, pt, &newPos.x, &newPos.y, &newSize.x, &newSize.y);
	else
		flushScaleImage(frame, pt, &newPos.x, &newPos.y, &newSize.x, &newSize.y, scaleVal);

	// Transfer the pos and size amounts into a single bounds rect
	newBounds = Common::Rect(newPos.x, newPos.y, newPos.x + newSize.x, newPos.y + newSize.y);
}

void Screen::blockMove(const Common::Rect &r) {
	Common::Rect bounds = r;
	slamRect(bounds);
}

void Screen::blockMove() {
	blockMove(Common::Rect(0, 0, width(), height()));
}

void Screen::print(const Common::Point &pt, uint color, const char *formatStr, ...) {
	// Create the string to display
	va_list args;
	va_start(args, formatStr);
	Common::String str = Common::String::vformat(formatStr, args);
	va_end(args);

	// Figure out area to draw text in
	Common::Point pos = pt;
	int width_ = stringWidth(str);
	pos.y--;		// Font is always drawing one line higher
	if (!pos.x)
		// Center text horizontally
		pos.x = (this->width() - width_) / 2;

	Common::Rect textBounds(pos.x, pos.y, pos.x + width_, pos.y + fontHeight());
	if (textBounds.right > this->width())
		textBounds.moveTo(this->width() - width_, textBounds.top);
	if (textBounds.bottom > this->height())
		textBounds.moveTo(textBounds.left, this->height() - fontHeight());

	// Write out the string at the given position
	writeString(str, Common::Point(textBounds.left, textBounds.top), color);

	// Copy the affected area to the screen
	slamRect(textBounds);
}

void Screen::gPrint(const Common::Point &pt, uint color, const char *formatStr, ...) {
	// Create the string to display
	va_list args;
	va_start(args, formatStr);
	Common::String str = Common::String::vformat(formatStr, args);
	va_end(args);

	// Print the text
	writeString(str, pt, color);
}

void Screen::writeString(const Common::String &str, const Common::Point &pt, uint overrideColor) {
	Fonts::writeString(&_backBuffer, str, pt, overrideColor);
}

void Screen::vgaBar(const Common::Rect &r, int color) {
	_backBuffer.fillRect(r, color);
	slamRect(r);
}

void Screen::setDisplayBounds(const Common::Rect &r) {
	_backBuffer.create(_backBuffer1, r);
	assert(_backBuffer.width()  == r.width());
	assert(_backBuffer.height() == r.height());
}

void Screen::resetDisplayBounds() {
	_backBuffer.create(_backBuffer1, _backBuffer1.getBounds());
}

Common::Rect Screen::getDisplayBounds() {
	return _backBuffer.getBounds();
}

void Screen::synchronize(Serializer &s) {
	int fontNumb = fontNumber();
	s.syncAsByte(fontNumb);
	if (s.isLoading())
		setFont(fontNumb);
}

void Screen::initPaletteFade(int bytesToRead) {
	Common::copy(&_cMap[0], &_cMap[Graphics::PALETTE_SIZE], &_sMap[0]);
	Common::copy(&_cMap[0], &_cMap[Graphics::PALETTE_SIZE], &_tMap[0]);

	// Set how many bytes need to be read / have been read
	_fadeBytesRead = 0;
	_fadeBytesToRead = bytesToRead;
	_oldFadePercent = 0;
}

int Screen::fadeRead(Common::SeekableReadStream &stream, byte *buf, int totalSize) {
	warning("TODO: fadeRead");
	stream.read(buf, totalSize);
	return totalSize;
}

void Screen::translatePalette(byte palette[Graphics::PALETTE_SIZE]) {
	for (int idx = 0; idx < Graphics::PALETTE_SIZE; ++idx)
		palette[idx] = PALETTE_6BIT_TO_8BIT(palette[idx]);
}

} // End of namespace Sherlock

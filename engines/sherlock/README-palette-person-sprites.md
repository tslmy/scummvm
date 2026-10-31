# Palette-qualified Rose Tattoo person sprites

This is an experimental PC Rose Tattoo hires-person lookup. It is disabled by
default. Set `SCUMMVM_SHERLOCK_TATTOO_PALETTE_SAFE_SPRITES=1` together with
`SCUMMVM_SHERLOCK_TATTOO_ASSET_OVERRIDES`, a hires scale (typically `2`), and a
true-color hires format (`rgb565` or `rgba32`). With the flag unset, the
existing person sprite pack lookup is unchanged.

For each visible person, including alternate image owners, the opt-in lookup
requires this exact asset layout (the scale shown is 2):

```text
sprites/<resource>_vgs/variants/<resource-md5>-s0a0/palettes/<projection-md5>/
  palette.bin
  frame_000@2x.png
  ...
```

`resource-md5` is the full lowercase MD5 of the actual decompressed
`ImageFile` stream, not the name of the library it happened to come from.
The suffix describes the image decoder flags: `s0a0` means `skipPal=false`,
`animImages=false`; other modes use their actual `s` and `a` bits. Frame
numbers are the original `ImageFile` indices. MD5 here identifies content; it
is not an authentication mechanism.

`palette.bin` is exactly the following bytes:

* the 8-byte magic `RT-PAL1\0`;
* 32 bytes containing the union of all decoded frame indices used by this
  file, with index `i` represented by bit `i % 8` of byte `i / 8`;
* the current installed 8-bit RGB triple for each set index, in ascending
  index order.

Person drawing uses index 255 for transparency; do not include it in the
union. Index 0 is opaque and must be included if used. `projection-md5` is the
full lowercase MD5 of the complete payload above. A room's initial palette
is only a candidate: the engine compares against the palette actually
installed at the time of drawing, including partial updates. The sidecar is
read and compared byte-for-byte, even when a decoded PNG is cached. Missing,
short, malformed, or mismatching sidecars, missing/invalid PNGs, a wrong PNG
size, unavailable identity, or a missing projection all use the native
person frame. There is no name-only fallback in opt-in mode. PNG hits and
misses are cached by resource, full palette payload, frame, and dimensions;
relaunch after editing images in a pack.

When preparing a pack, preserve the complete `variants/.../palettes/...`
directory structure and copy each projection's `palette.bin` alongside its
upscaled `frame_NNN@2x.png` files. An upscaler that emits PNGs but omits the
sidecar will produce native fallbacks, not a partially trusted pack. Initial
RRM palettes can be converted to RGB8 for candidate projections, but must not
be substituted for the runtime palette when deciding whether to display one.

This does not validate the artwork's visual fidelity or provenance beyond
the supplied sidecar and expected dimensions. In particular, it does not fix
palette fades or changes occurring after a scene layer was drawn, existing
cached RGB layers, cursors, or scene haze. Other objects, animations, UI,
backgrounds, and game logic are outside this switch. A pack generated from
initial room palettes will safely fall back to native whenever the current
palette has no matching projection; it is not a guarantee of uninterrupted
hires artwork during dynamic effects.

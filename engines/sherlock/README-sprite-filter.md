# Experimental Rose Tattoo sprite filtering

Set `SCUMMVM_SHERLOCK_TATTOO_SPRITE_FILTER` before launching to choose how
perspective-scaled high-resolution sprite overrides are sampled:

- `bilinear`: interpolate the entire override.
- `edge`: retain nearest-neighbor pixels when both nearest and interpolated
  alpha are fully opaque; otherwise use the same bilinear sample. This aims to
  preserve opaque interior detail while softening transparent boundaries.
- Unset or any other value: use the original nearest-neighbor rendering.

These options require the existing hires scale, true-color format and override
assets; they do not filter native indexed fallback, backgrounds, cursors or text.

Both filtered modes sample source pixel centers with edge clamping and
interpolate RGB in premultiplied-alpha space before returning straight alpha to
the compositor. The edge mode's interpolated alpha, including 8-bit rounding,
must be fully opaque before it can return a fully opaque nearest pixel.
An unscaled override is copied exactly, including with horizontal flip. Draw
geometry, native sampling, provenance and draw order are unchanged. Existing
native-resolution provenance can clip some newly smoothed edge pixels. Bilinear
minification is not an area filter and may still alias at large reductions.

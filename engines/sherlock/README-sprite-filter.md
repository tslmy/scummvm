# Experimental Rose Tattoo sprite filtering

Set `SCUMMVM_SHERLOCK_TATTOO_SPRITE_FILTER` before launching to choose how
perspective-scaled high-resolution sprite overrides are sampled:

- `bilinear`: interpolate the entire override.
- `edge`: retain nearest-neighbor RGB in fully opaque interiors and use
  bilinear RGB at low alpha, with a small high-opacity ramp between them. This
  aims to preserve opaque detail while softening transparent boundaries.
- Unset or any other value: use the original nearest-neighbor rendering.

These options require the existing hires scale, true-color format and override
assets; they do not filter native indexed fallback, backgrounds, cursors or text.

Both filtered modes sample source pixel centers with edge clamping and
interpolate RGB in premultiplied-alpha space before returning straight alpha to
the compositor. Edge mode keeps the bilinear alpha unchanged and uses the
smaller of nearest and interpolated alpha to choose its RGB: at 223/255 or
below it is entirely bilinear; from 224 through 254 it linearly transitions
toward nearest RGB; at 255 it is exactly nearest. Thus low-opacity or hidden
nearest RGB never contributes. This ramp reduces threshold jumps but cannot
guarantee temporal stability when nearest texels or animation frames change.
An unscaled override is copied exactly, including with horizontal flip. Draw
geometry, native sampling, provenance and draw order are unchanged. Existing
native-resolution provenance can clip some newly smoothed edge pixels. Bilinear
minification is not an area filter and may still alias at large reductions.

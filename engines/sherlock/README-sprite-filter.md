# Experimental Rose Tattoo sprite filtering

Set `SCUMMVM_SHERLOCK_TATTOO_SPRITE_FILTER=bilinear` before launching to
interpolate perspective-scaled high-resolution sprite overrides. An unset or
other value keeps the original nearest-neighbor rendering. The option requires
the existing hires scale, true-color format and override assets; it does not
filter native indexed fallback, backgrounds, cursors or text.

The filter samples source pixel centers with edge clamping and interpolates RGB
in premultiplied-alpha space before returning straight alpha to the compositor.
An unscaled override is copied exactly, including with horizontal flip. Draw
geometry, native sampling, provenance and draw order are unchanged. Existing
native-resolution provenance can clip some newly smoothed edge pixels. Bilinear
minification is not an area filter and may still alias at large reductions.

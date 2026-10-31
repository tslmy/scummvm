# Rose Tattoo native-reference diagnostic

This diagnostic is for comparing the hires compositor with native rendering;
it does **not** implement hires haze or modernize the affected scenes. Set
`SCUMMVM_SHERLOCK_TATTOO_HIRES_DEBUG=native-reference` together with a hires
scale greater than one. The native screen framebuffer is scaled nearest
neighbour using the installed palette. Hires backgrounds, scene sprites,
widget icons, text and cursor overrides are not presented; bitmap text and
the native scaled cursor remain enabled. The native-resolution background
override is also disabled. The choice is fixed for the process lifetime.

For example, set these in the environment before launching the patched engine:

```sh
SCUMMVM_SHERLOCK_TATTOO_HIRES_SCALE=2
SCUMMVM_SHERLOCK_TATTOO_HIRES_FORMAT=rgba32
SCUMMVM_SHERLOCK_TATTOO_HIRES_DEBUG=native-reference
export SCUMMVM_SHERLOCK_TATTOO_HIRES_SCALE SCUMMVM_SHERLOCK_TATTOO_HIRES_FORMAT SCUMMVM_SHERLOCK_TATTOO_HIRES_DEBUG
```

The input is the final native screen, so mask drawing, scene ordering, widget
pixels and scrolling are not reconstructed by the diagnostic. This differs
from the older `native` debug view, which still replays hires layers and uses
scene ownership checks. Normal rendering is unchanged unless explicitly
requested. Native text metrics can differ from TTF metrics, and captures taken
at different times can show different animation, scroll or story state.

For a comparison, use the same isolated copy of a save, game data and output
scale, then run once with this mode and once with the regular composite. Check
rooms 7, 8, 18, 53 and 68, including flag-dependent secondary masks, overlap,
scrolling, text and transitions. Never infer index-exact RGB haze from a PNG:
arbitrary true-color detail no longer has the source palette index needed by
the original lookup table. A palette-delta approximation must remain opt-in
and be evaluated separately against this reference.

The reference-frame unit tests verify copying and palette conversion, not the
native RLE decoder, game logic or interactions. Automated mask, transition and
UI coverage is still required before using a native fallback as a gameplay fix.

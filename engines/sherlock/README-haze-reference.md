# Rose Tattoo native-reference diagnostic

## Optional scene-scoped safety fallback

Set `SCUMMVM_SHERLOCK_TATTOO_HAZE_NATIVE_FALLBACK=1` before launch to use
native rendering in rooms 7, 8, 18, 53 and 68. Other rooms and the overhead map
continue using the configured renderer. This is a conservative playability
option, **not** a hires haze implementation or modernized art for those rooms.
It is disabled by default.

In those rooms the original indexed framebuffer is presented with native bitmap
text and without background, scene-sprite or widget-icon overrides. At hires
scales it is enlarged nearest-neighbour using the installed palette; at scale 1
the native-size `background.png` override is also bypassed. The journal stays
native while opened from one of these rooms, and the map restores its own
normal rendering before it starts. Cursor selection, visibility, hotspot and
input processing are not switched: a configured hires cursor remains in use.
As elsewhere, a true-color cursor does not necessarily follow palette fades.

The policy is chosen before the destination room's UI and resources are drawn;
retained hires layers are cleared when switching between native and enhanced
policies, and pending masked-scene widgets are discarded at those boundaries.
At hires scales, the whole-game `native-reference` debug mode below takes
precedence when both options are enabled.

Local validation used private game data and isolated save copies. Indexed
boundary fixtures matched the whole-game reference in both mask modes for all
five rooms and both conditional-flag states in rooms 8 and 18. Separate checks
covered a room 8 walk with scrolling and room 18 in CLUT8/RGB565/RGBA32. A 1x
fixture also checked that a native-size override is bypassed. Interaction
checks covered a hotspot description and magnifier cursor, inventory across
room changes, a journal round trip, a map location click, and return to an
enhanced room. Synthetic sprite and baked markers checked that old layers did
not survive a transition. TTF-present, TTF-absent and a no-FreeType build were
exercised. These are targeted smoke tests, not exhaustive coverage of every
story transition, platform, scaler, or scripted cutscene. Debug scene jumps do
not simulate natural entrances.

## Whole-game reference view

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

## Native mask trace

To capture the actual native mask operations without changing the displayed
compositor, set `SCUMMVM_SHERLOCK_TATTOO_MASK_TRACE` to a **new file path in a
private, isolated directory**. The trace contains game-derived RLE and indexed
pixels; do not commit or distribute it. Existing destination files and
`<destination>.tmp` files are refused. Do not have concurrent processes share a
destination: the existence check is not an exclusive file creation. ScummVM's
atomic writer can keep the data under `.tmp` until a clean exit; an interrupted
run may leave that file behind.

For example, after choosing an unused private directory:

```sh
export SCUMMVM_SHERLOCK_TATTOO_MASK_TRACE=/path/to/private/new-trace.rtmask
# Optional: 1..64 samples per mode per scene visit (default 2)
export SCUMMVM_SHERLOCK_TATTOO_MASK_TRACE_LIMIT=2
# Optional: capture the first invocation and every Nth thereafter (default 1)
export SCUMMVM_SHERLOCK_TATTOO_MASK_TRACE_STEP=1
# Launch the patched game with an isolated save copy; quit it cleanly.
python3 test/engines/sherlock/verify_mask_trace.py /path/to/private/new-trace.rtmask
```

The synthetic verifier tests can be run with
`python3 -m unittest discover -s test/engines/sherlock -p 'test_mask_trace_verifier.py'`;
the CxxTest suite also exercises trace collection and serialization.
For controlled runs using the same save copy and starting state, compare the
first reference and fallback samples for every recorded scene/mode/flag state:

```sh
python3 test/engines/sherlock/compare_haze_fixtures.py \
  /path/to/private/reference.rtmask /path/to/private/fallback.rtmask \
  --require-scenes 18 --require-both-modes
```

This compares indexed mask-boundary state, not the later presented UI or cursor.
Animation timing, scroll or story-state differences correctly make it fail;
compare matching capture points rather than assuming two wall-clock screenshots
represent the same game frame.

The trace can also be used with `native-reference` or with hires disabled. Each
sample contains the full world-sized indexed back buffer immediately before and
after `drawMaskArea`, its lookup table and installed display palette, scroll
and mask offsets, every native mask call and RLE pair, evaluated conditional
flag, and a per-pixel hit count. Samples are discarded in memory after writing;
a new scene visit resets sampling and receives a new epoch. Mode `false` is
recorded independently from mode `true`.

The verifier independently decodes the captured RLE, checks actual call order,
locations and flag-dependent secondary masks, compares its reconstructed hit
counts with the instrumented counts, and replays the lookup over **every**
recorded indexed pixel. It rejects invalid, incomplete or mismatching records.
Use `--require-scenes 7 8 18 53 68 --require-both-modes` across trace files to
require that coverage. To sample movement over a longer interval, raise `STEP`
and `LIMIT`; the trace does not capture intervening frames.

This is a snapshot of the buffer at the mask boundary, **not** a retained
presentation-layer ownership map. Later screen blits, widgets, hires-only
text and the backend cursor are not captured, and some restoration paths can
already have other content in the buffer. Do not apply an RGB effect to these
pixels as though the trace proved scene/UI ownership or exact RGB palette
semantics. Those require separate design and interaction tests. Snapshotting,
verification and file I/O can also change frame timing when tracing is enabled.

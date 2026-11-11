# Scalpel EQ 1.0.6 visual finish review

Baseline: verified upstream commit `4aa5efbb65459c8773bf3d7fc568b89b25338ff8`.
Branch: `ui-fidelity/aaa-finish-1.0.6`. No stable promotion or default-branch merge.

## Reference lock

The functional screenshot `image(20261110-040142).png` is 1350 x 785.
Its editor content is the 1344 x 756 rectangle at (3, 27); the remainder is
FL Studio title bar and window surround. Plugin code does not draw that chrome.
All existing logical bounds and the 1120 x 630 uniform coordinate system remain.
The comparison fixture selects BAND 3 / CARVE 4, 215.87 Hz, 0.00 dB depth,
Q 0.300 / 0.0% tightness, and Carve ON. Factory opening page remains BAND MODE.

The wide `Precision Scalpel EQ Console(1).png` guides material and lighting.
Its different knob diameters, panel proportions, bright exterior flares, and
extra presentation margin do not override the functional screenshot or the
user's restrictions on additional glow and components.

## Implementation

- `ScalpelFinish.h`: exact supplied material palette, deterministic cached
  software-backed grain, layered silver hardware, dark command switches,
  concentric graphite knobs, active ice indicators, and tabular digit drawing.
- `SurgeryLookAndFeel.h`: routes existing controls through the shared finish;
  state-specific text contrast; dark dropdowns; measured numeric baselines.
- `ScalpelUI105.h`: applies the materials to existing chassis, panels, frames,
  dividers, monitoring bay and meters at their original bounds.
- `RootsEditor.cpp`: display-only glass tones and response tint; existing
  mode state selects the presentation. Live FFT, Carve smoothing, parameter
  mappings, graph synchronization and audio code remain intact.

Static grain is created once in software memory. Chassis rendering continues
to use the editor's existing size/DPI cache. Knobs remain vector-native.
No full-screen artwork, model-generated textures, or scaled mockups are used.

## Verification order

First compile and inspect CARVE/COLOR captures, exact fixture values, unchanged
component bounds, contrast and state material identity at two display sizes.
Only after that gate passes, enable the same master finish for BAND/ANALYZE
and repeat native captures and the full regression/validation workflow.

`Tests/ScalpelFinishTests.h` produces actual Windows JUCE screenshots and a
component geometry CSV. `Tests/compare_aaa_finish.py` creates a matched-content
50% overlay, absolute difference image, three-way comparison and sampled
material report. Actual plugin pixels are never resized by that script.

Compilation and pluginval do not prove FL Studio verification or 1:1 visual
fidelity. Remaining visual differences and environment limitations must be
reported with the compiled evidence.

CARVE/COLOR gate passed on Windows in run `42925303416`, commit `ad56ed4`:
26 regression tests, fixture geometry/state/contrast checks and pluginval.
The first aligned comparison identified elevated knob-centre midtones; the
final radial stops lower that centre while retaining directional reflections.
Headings use 16 px Segoe UI bold and measured top-aligned glyph baselines
inside their existing boxes; numeric readouts use explicit tabular advances.

`Tests/ScalpelModuleCapture.h` additionally loads the built VST3 through JUCE's
VST3 host, sets the reference parameters through the real host interface,
clicks its native controls, and captures the embedded native window from the
Windows desktop. These are module-host captures, not editor-only snapshots.
It preserves 1344 x 756 dimensions on all four pages and restores any CI-only
display mode adjustment on exit. This host is JUCE, not FL Studio.

The all-page native-module capture passed in run `42926779985`. That run
also exposed an obsolete chassis assertion: pixels below 2% brightness were
classified as missing coverage, including deliberately painted black trim.
The corrected gate checks opaque coverage and the presence of the bezel at
the locked physical edge, at every supported size/DPI. It does not brighten
the approved black material to satisfy a palette-dependent threshold.

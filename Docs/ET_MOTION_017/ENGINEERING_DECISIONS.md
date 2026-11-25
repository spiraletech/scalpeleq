# ET-MOTION-017 — AURA dimensional symmetry

Baseline: spiraletech/scalpeleq, a491e2ce565036b9e121fb5ceaa8addf9fb9d910
on ui-fidelity/aura-omnis-r03-et-motion-016. The baseline was clean locally and
matched the remote reference. Work is isolated on
ui-fidelity/scalpel-2-dimensions-et-motion-017. A source archive and per-file hashes
were preserved before edits; the original dirty workspace289-file record matches.
Product version remains1.0.8.7; Scalpel2.0 is the research designation.

## Canon and geometry

LAB075's earlier338px screen /831px editor study remains historical research;
its later accepted105% rotary correction is preserved. LAB078's1344x786 default,
53% placement and LAB079's combined button geometry supersede the earlier height
exploration for this implementation. LAB076 supplies the rendered carousel sizes.

The logical canvas is1280x746.571429. At the default window its uniform scale is
1.05, with1.05px vertical registration inset, as on the prior default. Increasing
only window height while retaining the16:9 constrainer would have letterboxed the
old lower screen; the new constrainer is224:131. Bounds896x524 to1792x1048 preserve
the established min/max widths and relative scales. No mode changes window size.
Analyzer296 logical units remains310.8px; lower screen grows250 to278.571429
logical units, yielding292.5px. C01 bounds/proportions/assets/motion are untouched.

One RotaryGeometry holds the105% enlargement: outer61.95, inner51.45, thickness
3.675, pointer40.95..63, pointer thickness1.575. All six callers pass that same
radius. Numerical fonts remain24 logical units; the105% change is not compounded
by enlarging sliders or numerical typography. Native attachment normalization,
value precision, wheel/drag behavior and host gesture/state behavior are retained.

Rotary labels are explicitly aligned by glyph baseline568.142857. All dial centers
are634.142857; numerical baselines709.142857. Integer native slider rectangles
use a common fractional renderer offset, avoiding per-mode baseline rounding.

ON/OFF and ANALYZE toggles share143x46 envelopes. Function buttons use154x47.
Integer native bounds plus a fractional translation preserve exact accepted
centers and synchronize drawing/hit testing. The carousel hit envelope is231x46
logical units; its inset rendered body is230.476190x45.714286, yielding242x48px.
This deliberately distinguishes a safe native hit envelope from the optical body.

## Carousel registration decision

Recovered research confirms dimensions, linked offsets and separation, but did
not supply literal LAB079 absolute carousel center coordinates. Registration is
explicitly derived: retain the native reference column X762, apply the accepted
rendered X offset-5/1.05; anchor the first row to accepted ON center610.142857 and
apply Y offset-17/1.05. Thus centerX757.238095, compressionY593.952381,
ratioY683.476190. Their rendered separation is94px. FORM should review this
engineering registration against the original combined laboratory specimen.
Values/fonts/arrow lengths are converted from rendered reference units once.

## ANALYZE and glyph source boundary

HEAT MAP and FREEZE now occupy the same stacked left-hand family as ON/OFF.
Labels and26-unit glyphs are composited inside each button. Both remain independent
toggles; Freeze's existing internal RESUME action is retained while its stable
functional face label remains FREEZE. Detached label components are hidden.
Visual Decay uses the CARVE middle grid centerX568 and the universal renderer.
No planning annotation or decorative future-function control is painted.

The exact LAB043 IG02 Razor and LAB044 CR02 Absolute Zero vector paths were not
recovered from the repository, research context or file search. The supplied JPEG
is visual guidance, not a recoverable exact original vector. Existing native paths
are retained unchanged and exact glyph fidelity remains BLOCKED. No stock icon,
invented contour or relabeled approximation is promoted as the approved research.
Supply the approved SVG/path/experiment source to close this gate.

Existing analyzer functions are HEAT MAP (aura_color), FREEZE (editor-local measured
history hold), VISUAL DECAY (aura_memory), X-RAY, peak hold/reset and calibrated
0..-60dBFS axes. Legacy analyzer slope/depth values exist for compatibility but do
not establish independently supported SMOOTHING/DISPLAY RANGE/TRACE/FILL/PEAK/GRID
controls. Those proposals remain reserved, not nonfunctional buttons or new DSP.

## MONOLITH and cache ownership

The content area grows166 to194.571429 logical units. Capture allocation, clipping,
five slice heights, phosphor rail, repaint bounds and shield derive from it.
Caches use current physical/layout scale; resizes/DPI paints refresh the incoming
raster and resample the frozen outgoing image without restarting the625ms
trajectory. An interrupted request freezes the current presented image into a
registered expanded raster. Navigation/analyzer/C01 remain outside the transformed
content. No extra timer, animation counter, selection authority or DSP callback is
introduced. Native physical host/DPI behavior remains an independent test gate.

## Verification boundary

Native/Linux tests and captures do not establish Windows VST3/pluginval or
FL Studio25 behavior. Each result is recorded separately in the final receipt.
The ET017 native regression compares all six ring renderings at an identical
normalized value, measures actual transformed envelopes, tests independent toggle
semantics, and interrupts/resizes the expanded shield. Existing parameter/audio
regressions and protected C01 gates remain required. Missing source or unperformed
host checks must never be marked verified.

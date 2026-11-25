# ET-MOTION-016 — development decisions

Product version remains 1.0.8.7. Publication is not authorized.

## Baseline and recovery

Repository: spiraletech/scalpeleq. Integration baseline is C01 review commit
2f73a3056ede3d27434692231fed12423336dacd, branch
ui-fidelity/aura-c01-r02-et-motion-014. Both recorded remote heads were inspected;
they match the receipt. Historical Windows run 45562542671 was queried live and
still reports success for this exact commit. Its results are historical evidence,
not OMNIS verification.

Development worktree: scalpel-omnis-et016, branch
ui-fidelity/aura-omnis-r03-et-motion-016. The original 50f400e workspace's 289
recorded hashes, HEAD and porcelain status matched before work. A baseline
archive and file hashes were written outside both worktrees before modification.
No reset, cleanup, force push, release operation or version increment is used.
One mistaken command wrote a graph presentation edit to the clean C01 worktree;
its exact isolated diff was captured, reversed with a checked reverse patch, and
applied to OMNIS. No preexisting changes were overwritten. Subsequent checks
must confirm both protected worktrees match their initial records.

## Authority and gaps

The Captain's receipt supplies the operative geometry, material calibration,
typography, rotary and timing requirements. Context recovery corroborated accepted
LAB-070, protected C01, and suspended LAB-073/074. It did not return the original
LAB-043/044 vector paths, LAB-065 runnable specimen, or complete LAB-061 synthesis
source. Existing native flame/snowflake contours are retained, at the requested
26 logical px and white illumination. Their exact research-contour fidelity is
BLOCKED. No claim of a reconstructed approved glyph is made.

SOMATIC is BLOCKED: no existing independent host-safe UI audio sink was found.
Starting an additional hardware audio device from a VST editor would violate
the no-uncontrolled-device-initialization requirement. No synthesized feedback,
dummy mute control, DSP voice, new parameter, or device initialization is added.

PHOTON, NERVE and cursive typography remain absent. Additional analyzer controls
are not manufactured as placeholders. Display slope and depth exist as legacy
parameters but the graph intentionally fixes calibrated 0..-60 dBFS and zero
slope. TRACE/FILL/PEAK/GRID have no matching independent control contract;
adding them requires another assignment. Existing HEAT MAP/FREEZE/DECAY remain.

## Geometry and protected C01

All new UI positions use 1280x718 through one centered, uniform transform. The
host editor retains 1344x756 and its existing 16:9 resize limits. The reference
aspect ratio differs slightly from 16:9: at 1344x756 the design occupies
1344x753.9, centered with 1.05 px top/bottom inset. No stretching is introduced.

C01 retains its native 143x45 bounds and original spacings on the 1120-wide
protected grid. A separate centrally derived transform preserves its existing
physical size (1.2 at the default editor), with vertical translation only to the
OMNIS strip centerline. Using 143 units at the new 1.05 scale would have shrunk
approved selectors by 12.5%; that was deliberately avoided. No C01 renderer,
motion source, asset, atlas or label path is edited.

## Shared presentation

ScalpelOmnisModel.h holds deterministic rotary endpoint math and bounded visual
trajectories. ScalpelOmnis.h holds the logical scene, Arial-family resolution,
universal rotary/numeric/OLED renderers and material profiles. SurgeryLookAndFeel
opts into OMNIS only for RootsEditor; existing attachments remain authoritative.
No duplicate parameter/controller state is introduced.

All six visible rotaries use exactly the same renderer. The 135..405 degree
cos/sin convention corresponds to JUCE 225..495 degree rotary gestures. Zero
lights no segments; positive values round to the nearest endpoint, including
segment zero; midpoint lights 17; maximum lights all 33. Bipolar zero is the
native normalized midpoint. Logarithmic CARVE attachments retain their existing
UI normalization while preserving host parameter skew and raw values.

Numbers use fixed tabular cells, a fixed unit anchor and a common baseline at
Y694. Label baselines are optically centered around the specified Y554; rings
share center Y619. Editable text still uses JUCE's original Slider text box and
attachment callbacks. No khaki field or seven-segment raster is composited.

Arial Black and Arial are not installed in the Linux environment. Diagnostic
captures explicitly use Liberation Sans; they cannot pass the Windows product
typography gate. Production code prefers the approved names when present.

## Motion and lifecycle

The existing 60 Hz editor timer samples all new time trajectories. C01's own
160 ms implementation is untouched. Graph visibility is editor-local, starts ON,
and never calls bypass/mute/suspend or changes parameters. Only viewport pixels
are transformed; axes and metering remain stationary and live. CRT progress uses
260 ms and supports continuous reversal without completion callbacks.

MONOLITH uses one 625 ms controller (260/105/260), five bounded slices, cached
outgoing/incoming native content and a content-only input shield. Navigation is
live and stationary. Rapid retarget freezes the currently presented image before
starting the next bounded trajectory; it does not queue mode-change callbacks.
Mode bindings switch immediately, presentation resolves to the latest request.
The existing sonar_reduced_motion parameter controls reduced motion, default ON.
No new host parameter is created. UI caches die with the editor; no background
animation callback captures a destroyed editor.

## Material translation and limits

All four LAB-069 numeric profiles are preserved as artistic metadata in source.
Implemented: depth -> inner dark boundary, contact shadow -> well opacity,
absorption -> base luminance, reflection -> broad diffuse light field, edge ->
boundary opacity, internal illumination -> base gradient, micro-bevel -> line
width; roughness -> diffuse field spread; curvature -> gradient knee; thickness ->
inner interface width; parallax -> registered static optical offset. Shared
ambient, direction, strength and neutral temperature affect luminance, field
orientation/opacity and a restrained neutral chromatic bias. These are engineering
translations, not physical shader equivalence. Dynamic viewpoint parallax and
full seven/twelve-stratum optical equivalence are not verified. Material
fidelity remains IN PROGRESS, subject to FORM inspection of actual captures.

## Verification boundary

Local Linux compilation/captures/tests are separate from Windows, pluginval and
FL Studio gates. The unchanged Windows workflow needs a pushed commit to build
this candidate. ET-MOTION-016 does not authorize publishing it. No previous VST3
or CI run is substituted for the unbuilt Windows candidate.

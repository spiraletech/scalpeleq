# Scalpel EQ 1.0.7.8 Technical & User Manual — development draft

**Status: automated-gate-passing 1.0.7.8 candidate draft. Canonical promotion is not authorized.**

This describes the integrated 1.0.7.8 candidate: retargetable spectral removal,
magnetic editing, independent band presence, legacy state migration and analyzer
corrections. Windows CTest and pluginval pass; actual FL Studio 25 validation is
still required. Do not treat the candidate as the canonical stable replacement.
Source code takes precedence over older
product descriptions, including the obsolete PHASE description in
`docs/SCALPEL_EQ_DSP_SPEC.md`.

## 1. Introduction and processing philosophy

Scalpel processes frequency territories using two frequency handles per band,
per-territory gain and effects, and four subordinate CARVE slots. Territory
processing has filter skirts and phase consequences. It does not separate
instruments or guarantee a mathematically isolated frequency interval.

The approved BAND / CARVE / COLOR / ANALYZE interface is unchanged by this
work. Audio correctness and deterministic state recall remain release gates.

## 2. Installation and FL Studio integration

An automated-gate-passing 1.0.7.8 Windows candidate is supplied separately.
Close the DAW and preserve your working bundle before copying
the complete `Scalpel EQ.vst3` folder into
`C:\Program Files\Common Files\VST3\`. Retain the nested
`Contents\x86_64-win\Scalpel EQ.vst3` binary. Rescan in FL Studio's plugin
manager, insert on a test mixer track, and verify playback and project recall
before using it on important work.

The source retains manufacturer `SpEt`, plugin code `ScE1`, and bundle ID
`com.spiraletech.scalpeleq`. Actual FL Studio 25 validation has not occurred.

## 3. Band creation, removal and selection

New sessions open with BAND 1 present, selected and immediately editable through
its two visible magnets. Double-click unoccupied graph space to create another
region in the first available slot. If every band is removed, the graph displays
the add-band gesture. Right-click a region and choose **REMOVE BAND** to return that slot.
Removal hides both magnets and bypasses all processing for that band.

BAND OFF remains spectral removal and is separate from deletion. Projects saved
before 1.0.7.8 migrate their seven territories to present so existing sessions
do not become empty merely because they lack the new presence parameters.

## 4. Dual magnets and the logarithmic grid

Each present band owns a left and right frequency magnet. A magnet edits only
frequency. It cannot cross its partner or enter an unrelated same-domain region.
Magnets snap to the logarithmic grid and nearby band boundaries. Hysteresis keeps
the selected target stable while the pointer remains near it. Hold Ctrl while
dragging for precise unsnapped placement.

Drag anywhere in the translucent interior to move the complete band while
preserving logarithmic width. This removes reliance on a small center dot.

## 5. Connected bands and group movement

Snapping one same-domain magnet exactly onto another creates a reciprocal,
serialized connection. Dragging a shared boundary resizes both neighboring
regions at the exact shared coordinate. Dragging any connected interior moves
the complete connected group while preserving each width and every shared edge.
The group stops as a unit at 20 Hz, 20 kHz or an unrelated same-domain region.

Right-click a connected band and choose **UNLINK LEFT MAGNET** or
**UNLINK RIGHT MAGNET**. Connections never break merely because the pointer is
dragged harder. Stereo, Mid and Side maintain separate collision spaces, so
deliberate Mid/Side frequency overlap remains legal.

## 6. Spectral removal and limitations

BAND OFF disables a territory's additive effects and blends in a 24-pole
Butterworth band-stop in the assigned channel domain. The routing fix makes
MID removal preserve Side and SIDE removal preserve Mid when other effects
are neutral. Stereo removal still processes both components. Amount changes
use the existing approximately 10 ms smoothing.

The integrated design keeps two fixed filter banks and uses a 20 ms equal-power
transition when boundaries change. This prevents the dangerous state/coefficient
mismatch measured in the rejected direct-modulation design. The tested interior
meets the preliminary 60 dB requirement across 44.1, 48, 96 and 192 kHz.

The exact filter edges are approximately −3.01 dB; “selected region” therefore
does not mean every point up to and including both mathematical edges is reduced
60 dB. Retarget transitions briefly contain both old and new responses, consume
twice the stopband filter work and can leak energy relative to the newest target
until the 20 ms transition completes. The process remains zero-lookahead with
reported latency of zero samples, but causal IIR phase/group delay still exists.

## 7. Stereo, Mid, Side and MONO

The engine encodes `M=(L+R)/2`, `S=(L−R)/2` and reconstructs `L=M+S`, `R=M−S`.
Per-territory gain follows the assigned Stereo/Mid/Side domain. The removal
routing correction is covered at 44.1, 48, 96 and 192 kHz.

MONO uses spectral Side suppression for the selected territory, with skirts;
it does not deliberately collapse the complete mix. During selected-band SOLO,
MONO drives that audition's Side toward zero. Interaction of MONO, PHASE,
nonlinear COLOR, multiple territories and non-stereo domains needs broader
validation. In particular, the baseline PHASE algorithm changes both Mid and
Side and is not proven to preserve a domain that was not selected.

## 8. SOLO and audition

One ROOT/BAND is authoritative for SOLO. The baseline crossfades audition
changes over approximately 8 ms. Mid SOLO excludes Side and Side SOLO excludes
Mid in the existing standalone tests. Its HP/LP taps have natural skirts:
SOLO is frequency audition, not source extraction or an ideal rectangular
selection. Additive effects, removal and rapid switching need host testing.

## 9. CARVE and C1–C4

Each territory owns four optional CARVE slots. Source parameters include
frequency, cut depth (0–36 dB), Q (0.3–96), threshold (−60–0 dBFS), dynamic
mode and channel assignment. Dynamic cuts use a detector with approximately
3 ms attack and 65 ms release. Frequency/depth updates are smoothed. Parent
territory/domain selection controls their applicable processing path. Boundary
weighting includes transition tails. These existing features have standalone
regressions; comprehensive host and automation verification is outstanding.

## 10. COLOR, compression, PHASE and LIMIT

COLOR comprises DYNAMIC, SPARKLE, SHINE, SMOOTHENING and VINTAGE TAPE. Zero
amounts are neutral in inherited standalone checks. Saturation is nonlinear
and is not certified alias-free. A static EQ line cannot describe these effects.

The FET-inspired compressor uses approximately 0.8 ms attack, 140 ms release,
4:1 ratio and a 2 dB knee. Threshold is read by the actual gain-reduction
calculation; it is not only a pointer value. Optical and tube profiles use
other time constants. This work does not certify an exact 1176 hardware model
or a full detector calibration across all profiles.

The current PHASE implementation estimates L/R phase using quadrature taps
near the geometric band center and rotates the R estimate toward L. An
inherited 350 Hz sinusoid test demonstrates reduced phase error with bounded
RMS change. Broadband, transient and all-domain safety are unverified; this is
not universal time alignment or recovery of cancelled information.

LIMIT is an optical-style leveler: about 10 ms envelope attack and
350–1000 ms program-dependent release. Amount maps to a soft reduction curve;
it is **not** a brickwall or true-peak limiter. The UI LIMIT switch uses amount
0.35 if its retained hidden amount parameter is near zero. Amount zero with the
switch on therefore does not represent bypass through that UI path.

## 11. Analyzer, X-RAY and metering

The graph draws measured spectrum and peak history independently from theoretical
response. ANALYZE adds cumulative static Stereo/Mid/Side paths for all present
linear band gains, CARVE filters and spectral-removal filters. Individual selected
CARVE curves remain separately colored. Compression, PHASE, dynamic CARVE and
nonlinear COLOR are not falsely represented as static transfer curves.

Hover shows frequency, musical pitch and **PEAK**. The old pointer-height dB
readout is removed. Spectral peak history holds for the full run until RESET;
RESET clears both output sample-peak and analyzer peak history. Visual Decay can
reach true zero, which follows each new measurement without release persistence.

X-RAY remains downstream of the analyzer and visual-only. A processor audio-null
test compares X-RAY enabled/disabled output.
Input/output sample peak is not a true-peak measurement.

## 12. Automation and preset recall

Existing parameter IDs, order and plugin identifiers remain unchanged. Presence
and two link parameters per band are appended. Frequencies and reciprocal links
fully reconstruct the arrangement from APVTS XML state. Missing presence values
migrate as legacy-present; missing links migrate unlinked. New-session defaults
remain absent. Actual FL Studio automation, undo/redo and project restoration
pass generic pluginval state-restoration and automation checks. Actual FL Studio
automation, undo/redo and project restoration still require host validation.

## 13. Safety, troubleshooting and known limitations

Keep using the approved working build for production. If a development test
produces unexpected gain or nonfinite samples, stop that test and retain the
reproducible log. Never connect the direct-modulation experiment to monitoring.

If a cut does not fully remove a sound, first distinguish the measured legacy
notch limitation from source separation: a sound may span many frequencies.
If an old project changes sound, compare against the exact approved binary;
the Mid/Side removal correction intentionally changes previously defective
cross-domain attenuation. No migration has been certified for broader redesigns.

The local full JUCE configure attempt failed because FreeType/X11 development
headers were unavailable. Portable DSP checks are independent of those GUI
build dependencies and do not establish VST3 or host reliability.

## 14. Verification and technical specifications

| Item | Evidence/status |
| --- | --- |
| Baseline | `ad546dcc1c829a83d894d7f8fe94b55c5c131d79` |
| Framework/toolchain target | JUCE 8.0.4, C++20, Windows x64 VST3 |
| Existing declared latency | 0 samples; IIR group delay still exists |
| Sample rates probed | 44.1, 48, 96, 192 kHz |
| Portable tests | See `evidence/1.0.7.8/patched-ctest.log` |
| Production spectral gate | PASS on integrated path |
| Mid/Side removal fix | PASS; see `patched-domain.log` |
| Direct coefficient-modulation design | Rejected; retained as historical evidence |
| Crossfaded retarget path | Integrated; live-drag safety gate passes |
| Windows x64 build / native VST3 capture | PASS on candidate `787e186b87274e746f1be70b615eb34fc0e3038e` |
| Windows CTest | 35/35 pass in run 219 |
| pluginval | SUCCESS at strictness 8, seed 12345; GUI tests skipped |
| FL Studio 25 | Not tested in this environment |
| Magnetic interactions | Portable/native relationship and invariant tests pass; actual host interaction pending |

Reproduce standalone checks with:

```sh
cmake -S . -B build-gates -DSCALPEL_PORTABLE_TESTS_ONLY=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-gates --parallel 2
ctest --test-dir build-gates --output-on-failure
```

Passing automated gates are evidence for a candidate, not permission to promote
the release. Actual FL Studio findings remain a separate required record.

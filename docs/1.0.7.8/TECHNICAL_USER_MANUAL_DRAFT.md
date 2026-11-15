# Scalpel EQ 1.0.7.8 Technical & User Manual — development draft

**Status: Phase B held at a failed production audio gate. Not a release manual.**

This describes the inspected 1.0.7.7 implementation plus the isolated 1.0.7.8
routing fix. Magnetic editing, band presence, migration and analyzer changes
have not been implemented. Do not install this source branch as a stable
replacement. Version metadata intentionally remains 1.0.7.7 until an integrated
candidate earns the 1.0.7.8 designation. Source code takes precedence over older
product descriptions, including the obsolete PHASE description in
`docs/SCALPEL_EQ_DSP_SPEC.md`.

## 1. Introduction and processing philosophy

Scalpel processes frequency territories using two frequency handles per band,
per-territory gain and effects, and four subordinate CARVE slots. Territory
processing has filter skirts and phase consequences. It does not separate
instruments or guarantee a mathematically isolated frequency interval.

The approved BAND / CARVE / COLOR / ANALYZE interface is unchanged by this
work. Audio correctness is the release gate for subsequent interaction work.

## 2. Installation and FL Studio integration

No validated 1.0.7.8 Windows VST3 is supplied at this checkpoint. For a future
validated build, close the DAW and preserve your working bundle before copying
the complete `Scalpel EQ.vst3` folder into
`C:\Program Files\Common Files\VST3\`. Retain the nested
`Contents\x86_64-win\Scalpel EQ.vst3` binary. Rescan in FL Studio's plugin
manager, insert on a test mixer track, and verify playback and project recall
before using it on important work.

The source retains manufacturer `SpEt`, plugin code `ScE1`, and bundle ID
`com.spiraletech.scalpeleq`. Actual FL Studio 25 validation has not occurred.

## 3. Band creation, removal and selection

**Current:** seven existing territory slots and fourteen boundary parameters.
Select a territory/handle using the existing graph and BAND selectors.

**Not implemented:** an empty new-session graph; band presence independent of
BAND OFF; double-click creation; removal and slot reuse. BAND OFF remains a
spectral-removal command. It is not deletion. No new presence state or legacy
migration has been introduced in this checkpoint.

## 4. Dual magnets and the logarithmic grid

**Current:** handles edit frequency only; vertical movement does not set gain.
The engine derives each interval from the minimum and maximum handle
frequencies. This is not a magnetic constraint model.

**Not implemented:** noncrossing anchors, grid snapping, snap hysteresis,
shared-anchor ownership, fine-mode overrides for the proposed magnet system,
and translucent-interior movement.

## 5. Connected bands and group movement

**Not implemented.** Touching boundaries in the baseline are numeric
coincidences, not persistent connections. There is no group traversal,
whole-group clamp or explicit unlink command. None should be advertised.

## 6. Spectral removal and limitations

BAND OFF disables a territory's additive effects and blends in two cascaded
notch sections in the assigned channel domain. The isolated routing fix makes
MID removal preserve Side and SIDE removal preserve Mid when other effects
are neutral. Stereo removal still processes both components. Amount changes
use the existing approximately 10 ms smoothing.

The production topology fails the new rejection requirement: probes at 25%
and 75% of logarithmic interval width show roughly 14–16 dB attenuation in
the tested cases, even when the center is deeply attenuated. Boundary rejection
is weaker. Do not interpret BAND OFF as complete removal of the full region.

An unconnected 24-pole Butterworth experiment has approximately −3 dB edges
and much deeper interior rejection. Direct coefficient changes cause dangerous
transients and must not be integrated. A two-bank, 20 ms crossfade reduces those
spikes in one tested drag trajectory, but blends old and new responses, delays
target tracking, increases CPU, and temporarily leaks energy relative to the
latest selected interval. Neither experiment is a released processor.

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

The baseline graph draws measured spectrum and a peak trace, plus a selected
territory's four CARVE previews. These are not the full cumulative response.
BAND OFF suppression is not included in that theoretical preview. Measured
spectrum and theoretical EQ must not be conflated.

The hover still includes spectrum-bin dBFS and `HOLD` values; the requested
PEAK labeling and mouse-information cleanup are not implemented. RESET calls
the processor's output sample-peak reset. There is no verified full-song
spectral history or independent whole-song reset semantics.

Visual Decay / spectrum-release behavior has not been changed to true zero.
X-RAY remains the inherited visualization path; this patch adds no audio
connection to it. A new full-plugin X-RAY audio-null comparison is outstanding.
Input/output sample peak is not a true-peak measurement.

## 12. Automation and preset recall

Existing parameter IDs, order, defaults and plugin identifiers are unchanged.
No presence/link parameters or magnetic serialization format were added.
The existing APVTS XML state/preset mechanism and legacy sanitation remain.
Source compatibility is preserved by inspection; actual old-project recall,
automation, undo/redo and FL Studio session restoration are unverified.

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
| Production spectral gate | FAIL; see `patched-spectral.csv` |
| Mid/Side removal fix | PASS; see `patched-domain.log` |
| Direct coefficient-modulation experiment | FAIL audio safety |
| Crossfade experiment | Narrow trajectory test passes; integration unverified |
| Windows x64 build / native VST3 capture | PASS on tested code `e23cb6e` |
| Windows CTest | 32/34 pass; two audio release gates fail |
| pluginval / FL Studio | Skipped after failed gate / not tested |
| Magnetic interactions / migration | Not implemented; not tested |

Reproduce standalone checks with:

```sh
cmake -S . -B build-gates -DSCALPEL_PORTABLE_TESTS_ONLY=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-gates --parallel 2
ctest --test-dir build-gates --output-on-failure
```

The test command intentionally returns failure while the release gates fail.
Do not hide those failures with expected-failure flags. A passing subset is
not permission to promote a release.

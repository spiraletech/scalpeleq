# Scalpel EQ 1.0.7.8 magnetic DSP stabilization — candidate report

**Status:** controlled-development candidate. Automated gates pass; FL Studio 25
validation and canonical promotion remain outside this report.

## Baseline, branch and identity

- Approved baseline branch: `ui-fidelity/band-anchored-carve-1.0.7.7`
- Verified baseline commit: `ad546dcc1c829a83d894d7f8fe94b55c5c131d79`
- Isolated patch branch: `fix/scalpel-1.0.7.8-magnetic-dsp`
- Integrated code commit: `b24c5192f34b378c11ef105c80382c1667a4c5a5`
- Final candidate/package commit: `787e186b87274e746f1be70b615eb34fc0e3038e`
- Plugin identity retained: manufacturer `SpEt`, plugin code `ScE1`, bundle ID
  `com.spiraletech.scalpeleq`

No protected branch was modified. No merge, release, tag or canonical promotion
was performed. The approved 1.0.7.7 artwork/material baseline was not replaced.

## Implemented system

### Audio path

- BAND OFF now uses the integrated fixed-storage 24-pole Butterworth band-stop.
- Two preallocated banks crossfade over 20 ms when a boundary changes, avoiding
  the unstable coefficient/state mutation measured in the rejected experiment.
- Absent bands bypass their complete DSP path; BAND OFF remains spectral removal
  and is not used as a deletion surrogate.
- Mid removal preserves Side, Side removal preserves Mid, and Stereo removal
  applies to both components.
- Filter work is skipped for absent/fully enabled bands, with a one-time reset
  after the wet envelope reaches zero. No live-edit heap allocation is added.

### Magnetic relationship model

- Seven stable slots each carry presence, left/right frequency, domain and two
  reciprocal connection identifiers.
- New sessions start with BAND 1 present, selected and visibly editable; the
  other six slots remain absent.
- Double-click adds the first free slot; right-click removes only that band.
- Both magnets drag independently; the translucent interior moves a band.
- Same-domain collision rules prevent inversion/crossing while Mid/Side regions
  may overlap deliberately.
- Log-grid/neighbor snapping uses acquire/release hysteresis; Ctrl bypasses snap.
- Shared anchors use one exact frequency and reciprocal serialized links.
- Shared-edge dragging resizes adjacent bands; interior dragging moves the full
  connected group without breaking links or changing logarithmic widths.
- Groups clamp as a unit at the spectrum limit or an unrelated collision.
- Explicit right-click unlink operations replace accidental breakaway behavior.

### State and analyzer

- Historical parameter IDs and order are unchanged; presence/link parameters
  are appended.
- Legacy states missing the new fields migrate all seven territories as present
  and unlinked. New-session defaults remain absent.
- The analyzer draws cumulative static Stereo/Mid/Side response for all present
  linear band gains, CARVEs and spectral removal, while measured spectrum stays
  independent. Dynamic/nonlinear behavior is not mislabeled as static response.
- Hover uses PEAK and removes the pointer-height dB readout. RESET clears meter
  and analyzer history. Visual Decay reaches true zero. X-RAY remains audio-null.

## Measured audio evidence

The production-engine release gate covers 250–500 Hz, 950–1050 Hz and
100–10000 Hz regions at 44.1, 48, 96 and 192 kHz. Interior probes are at 25%,
50% and 75% of logarithmic width.

| Measurement | Result |
| --- | --- |
| Interior probes | 36/36 pass the −60 dB gate |
| Integrated interior range | −72.2539 to −157.359 dB (Mid); −72.2541 to −154.921 dB (Side) |
| Direct steady-sine candidate range | −72.2545 to −243.952 dB |
| Outside steady-sine probes | maximum absolute change 0.002541 dB in the tested set |
| Exact mathematical edges | approximately −3.0103 dB, reported rather than claimed removed |
| Mid/Side unselected-domain gate | 8/8 pass; measured change far below 0.001 dB |
| Retarget transition safety | 4/4 sample rates finite; peak 0.270304–0.282041 for 0.25 input |
| Latency | zero reported samples; causal IIR phase/group delay remains |

The stopband is not mathematically rectangular. The 60 dB claim applies to the
tested interior, not the exact boundaries or every possible extremal bandwidth.
During a 20 ms retarget transition, both old and new responses coexist and the
newest target can have temporary residual energy.

## Verification matrix

| Gate | Result |
| --- | --- |
| Portable CTest | 30/30 pass |
| Windows x64 Release build | pass |
| Windows native CTest | 35/35 pass |
| Native VST3 load/capture | pass inside CTest |
| pluginval 1.0.4 | SUCCESS, strictness 8, seed 12345, GUI tests skipped |
| Candidate VST3 bundle | produced and structurally checked |
| Actual FL Studio 25 | not tested in this environment |

Final Windows run: [GitHub Actions run 219](https://github.com/spiraletech/scalpeleq/actions/runs/43827684060)

Artifacts:

- Candidate VST3: artifact `14207946318`
  (`sha256:3eb58c6840bf725faccfa94578a7692d9839ebb24bb648bdd1c5eb6a2a53b863`)
- CTest/measurement evidence: artifact `14208276172`
  (`sha256:910e2fe88509d146c7504dcb3e578cb7ce37f7bf6d483a94393b6388ef73e0d3`)
- pluginval log: artifact `14207238455`
  (`sha256:4e4d0cae43b79bd4ecedb45e2cc005b741c44cb936c51893a88b1a7c3e77e02c`)

## Important limitations

- This is not a stable-release declaration. Agnathos approval is still required
  before merge, tag, release or replacement of 1.0.7.7.
- FL Studio 25 playback, automation recording, project recall, rapid editor
  interaction, editor closure and plugin removal require an actual host pass.
- pluginval was intentionally run without GUI tests; it does not substitute for
  FL Studio interaction validation.
- PHASE has a bounded inherited sinusoid proof but is not certified as universal
  broadband alignment. LIMIT is an optical leveler, not brickwall/true-peak.
- Compression threshold participates in real gain reduction, but the profiles
  are not certified as exact hardware emulations.
- Undo/redo safety relies on APVTS parameter transactions and invariant repair;
  a dedicated host-driven undo sequence remains unverified.

## Delivery map

| Required delivery | Delivered evidence |
| --- | --- |
| A. Branch and commits | Baseline/branch section above |
| B. Code-level summary | Implemented-system section and repository diff |
| C. Spectral evidence | Measurement table plus CI CSV artifact |
| D. Magnetic tests | `ScalpelBandModelTests.cpp`; native/portable CTest pass |
| E. Regression/VST3 logs | Final run and evidence/pluginval artifacts |
| F. FL Studio findings | Explicitly untested; no false claim |
| G. 1.0.7.8 VST3 | Candidate artifact, not a promoted stable build |
| H. Manual | `TECHNICAL_USER_MANUAL_DRAFT.md` |
| I. Limitations | Important-limitations section above |


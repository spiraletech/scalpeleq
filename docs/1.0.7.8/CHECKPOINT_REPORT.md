# 1.0.7.8 magnetic DSP patch — controlled-development checkpoint

**Decision: HOLD at Phase B. The requested patch is not complete or stable.**

## Baseline and isolation

The approved branch `ui-fidelity/band-anchored-carve-1.0.7.7` still resolved to
`ad546dcc1c829a83d894d7f8fe94b55c5c131d79` on this inspection. Source checkout
and all local work were isolated on `fix/scalpel-1.0.7.8-magnetic-dsp`.
The same remote patch branch already existed at the baseline; publication uses
an expected-head check and a fast-forward only. No protected branch was changed.

Published work commits preceding this report:

- `04c665ff11527d3a78bf18673015d4f736dcf406`: baseline tests and evidence.
- `c393eea7894d6cc924cc26bec7b313abc61da840`: Mid/Side removal routing fix.

The report's containing commit adds the experimental DSP, test integration,
manual and remaining evidence. Local staging commits have different hashes
because the authenticated GitHub service creates its own commit metadata;
content trees are checked during publication.

## What actually changed

| File | Change |
| --- | --- |
| `Source/RootsAudioEngine.h` | Domain-mask BAND OFF: Mid removal preserves Side; Side removal preserves Mid. Existing notch coefficients, stereo removal, MONO behavior, effects and smoothing otherwise retained. |
| `Source/Experimental/ScalpelBandStop.h` | Disconnected fixed-storage 24-pole Butterworth experiment and two-bank 20 ms crossfade experiment. Not included by PluginProcessor or RootsAudioEngine. |
| `Tests/ScalpelSpectralGateTests.cpp` | Full production-engine sine probes over interval positions and sample rates, plus channel-isolation gates; failures remain real failures in Release. |
| `Tests/ScalpelRemovalDomainTests.cpp` | Independent sample-domain preservation and selected-component attenuation checks. |
| `Tests/ScalpelBandStopExperiment.cpp` | Analytic response and actual impulse, white-noise, multitone, sweep and direct-drag tests; ordinary allocation audit. |
| `Tests/ScalpelBandStopMeasuredTests.cpp` | Actual steady sine attenuation, theoretical comparison and numeric group-delay estimates. |
| `Tests/ScalpelBandStopTransitionTests.cpp` | Separate crossfaded drag experiment; finite samples and conservative full-scale spike bound. |
| `cmake/PortableTests.cmake`, `CMakeLists.txt` | JUCE-independent CTest route and production-build release gates. Keep inherited assertion checks enabled in Release. |
| `.github/workflows/windows-vst3.yml` | Include this exact patch branch; retain measurement CSVs with regression logs; mark any permitted artifact experimental. |
| `scripts/run-portable-regressions.py` | Reproduce baseline standalone checks with GNU C++20 and assertions enabled. |
| `docs/1.0.7.8/TECHNICAL_USER_MANUAL_DRAFT.md` | Fourteen-section implementation-grounded draft; unfinished features identified. |
| `evidence/1.0.7.8/` | Baseline, patched, experiment, configure and sanitizer results. |

No editor geometry, palette, material, control artwork, parameter ID, parameter
order, plugin identifier or state schema changed. CMake version stays 1.0.7.7:
this checkpoint has not earned the 1.0.7.8 build designation.

## Audio evidence

All levels below are digital-amplitude ratios, not listening claims.
The production-engine harness uses a Mid sine plus quadrature Side sine,
2 s settling and 1 s measurement. It compares enabled neutral processing
against BAND OFF with other controls neutral. Regions: 250–500 Hz,
950–1050 Hz and 100–10000 Hz. Sample rates: 44100, 48000, 96000, 192000 Hz.
Positions are fractions of logarithmic interval width: 0, .01, .10, .25,
.50, .75, .90, .99, 1. The preliminary −60 dB interior gate applies to
.25, .50 and .75. It does not redefine the boundary as inaudible.

| Check | Baseline | Patched production engine |
| --- | --- | --- |
| Interior rejection failures | 24 / 36 | 24 / 36: unresolved |
| Mid/Side isolation failures | 8 / 8 | 0 / 8 |
| Neutral standalone baseline regressions | 24 / 24 pass | 24 / 24 pass |

The measured production interior range includes deep center cancellation
(about −155 to −157 dB) but only about −13.94 to −15.97 dB at the tested
quarter-width positions. A center-only test would conceal this failure.
The selected component's 350 Hz attenuation in routing tests is about
−61.76 dB; the unselected component's maximum sample error is 7.45e−9.
That single-frequency result is not an interval-wide rejection claim.

Raw files: `baseline-spectral.csv`, `patched-spectral.csv`,
`patched-domain.log`. Baseline and patched tests have the same spectral
limitation intentionally: the unsafe experimental topology was not substituted.

### Alternative architecture experiments

The experimental filter transforms a 12th-order Butterworth lowpass prototype
into a 24-pole bandstop, prewarps both edges, and uses 12 fixed-storage biquads.
There is no lookahead or explicit sample delay; phase and group delay remain.

- 128 analytic response probes; no failure of the preliminary interior goal.
- Actual steady sine interior probes: worst measured −72.2545 dB;
  deepest measured about −243.952 dB. Four seconds settling, one second measured.
- Exact edges are approximately −3.01 dB. The full selected closed interval
  is not removed to −60 dB. The test does not claim otherwise.
- Fixed-filter impulse/noise/multitone/sweep tests remained finite. These are
  basic diagnostics, not a comprehensive transient/alias specification.
- Direct coefficient modulation at 256-sample intervals with a 3 Hz sweep of
  octave-wide boundaries around a −12 dBFS 350 Hz input failed at all four
  sample rates: output peaks 4.84508, 4.31074, 3.97935, 4.00964 linear.
  This is an unacceptable roughly +12 to +14 dBFS output for a 0.25 input.
- The fixed-bank crossfade follow-up gave peaks 0.282041, 0.280432, 0.281699,
  0.270304 in the tested trajectory. It is a promising containment experiment,
  not a validated integration. Up to a fade interval of target lag, mixed
  responses, temporary residual energy and extra CPU remain to characterize.
- Direct experiment observed zero ordinary `operator new` allocations while
  configuring/processing the audited loop. This does not prove all allocations
  absent from the integrated plugin, or instrument platform/over-aligned allocators.
- Numeric outside-probe group-delay estimates reached about 12.14 ms in the
  tested set. Near response zeros group-delay estimates are ill-conditioned.
  This does not establish a global maximum or perceptual acceptability.

The production engine clamps frequency to `min(20000, .45*sampleRate)`;
at 44.1 kHz this is 19845 Hz. The experiment explicitly rejects invalid,
nonfinite, inverted or out-of-limit ranges. Extremal bandwidths and all edge
positions have not been certified. Phase, ringing, out-of-band tolerance,
seven-band worst-case CPU, live-retarget rejection and denormal performance
remain integration gates.

## Tests and host validation

The initial complete portable CTest run was 26 passed / 2 failed. An additional
measured-sine test passes. The final passing subset is 27 tests. Two explicit
release gates remain failed and are not marked WILL_FAIL or suppressed in CI.
`patched-ctest.log` preserves the full failing run; `final-portable-regression.log`
and `candidate-measured-ctest.log` record the completed passing additions.

The independent subset does not include the JUCE processor/editor/quality
suite. Full local JUCE configuration failed building juceaide because
`ft2build.h` and `X11/Xlib.h` were unavailable; see `native-configure.log`.
The machine is Linux, not a Windows/FL Studio workstation.

AddressSanitizer/UndefinedBehaviorSanitizer rerun of the domain regression
completed with leak detection disabled, because the first attempt's
LeakSanitizer reported that it cannot run under this environment's ptrace.
Both attempts are retained. This is a focused address/undefined-behavior check,
not full memory-safety or leak certification.

Windows x64 VST3 compilation, full CTest, pluginval, FL Studio 25 playback,
automation, preset/session restore, rapid editor use, removal and editor closure
are unverified at this checkpoint. Source-branch CI can exercise Windows checks;
its result must be recorded separately. No validated 1.0.7.8 VST3 artifact is
provided, and no generic test is represented as FL Studio evidence.

## Phase status and remaining work

| Phase | Status |
| --- | --- |
| A — architecture and regression baseline | Baseline mapped and portable regressions recorded; full JUCE/host baseline unavailable locally. |
| B — audio integrity | Domain defect fixed. Production rejection remains failed. Candidate experiments retained; not integrated. Other channel/dynamics features have inherited tests, not comprehensive new verification. |
| C — magnetic band engine | HELD: not implemented. No add/remove, shared anchors, explicit unlink, group movement, serialization or undo/redo results exist. |
| D — analyzer accuracy | HELD: cumulative response, removal overlay, hover, full-song history and decay corrections not implemented. |
| E — host validation and manual | Draft manual provided. Windows/host release validation incomplete. |

The exact next engineering task is to decide and test a transition-band contract
for spectral removal, validate a safe retarget architecture against that contract
and CPU budget, then integrate it behind explicit legacy-state migration tests.
Only after that gate should presence/state migration and the magnetic graph be
introduced. Existing session cuts must not silently acquire a different filter.

The directive explicitly permits retaining an experimental build and reporting
failure when reliable rejection has not been established. This checkpoint follows
that gate; it does not request promotion, merge, stable publication or canonical
replacement of 1.0.7.7.

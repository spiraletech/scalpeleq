# Scalpel EQ

Scalpel EQ is a dedicated JUCE 8 / C++20 VST3 from Spiral Ether Tech, derived from the independently preserved Surgery EQ 1.0.12 Standard+ implementation.

**Source pin:** `spiraletech/prismvst`, branch `improve/surgery-eq-quality-pass`, commit `f3a816060520b0c0e87be466ecdb4d2d34f654a8` (unmerged PR #17). Changes in Scalpel EQ must never be pushed to `prismvst`.

## Locked build goals

- Separate plugin identifier/name; 7 frequency bands × 4 CARVE slots, per-band processing, saved parameter compatibility where applicable.
- Powdery champagne-ivory 1980s voicemail-machine interface: matte ivory dials with top triangular pointers, no exterior notches, recessed black Phillips screws, tactile controls.
- X-RAY spectrum mode: tar-like black-mercury fluid, bubbles/fizz, restrained audio-driven synesthesia; display-only, 60 FPS target, Freeze and Reduced Motion.
- On-screen dB ticks: `0 -5 -10 -15 -20 -25 -30 -35 -40 -50 -60` (the measurement and signal-processing ranges remain independently precise).
- COLOR: DYNAMIC, SPARKLE, SHINE, SMOOTHENING, VINTAGE TAPE. Five independently mapped, per-band DSP controls, 0% true neutral, bounded processing.
- Per-band 1176 Rev E-inspired FET compression, PHASE coherence optimization, LA-2A-inspired program-dependent optical leveling with one LIMIT amount, and local ≥ −0.01 dBFS peak-warning pulse.
- Automated impulse/sine/noise, gain, harmonic, DC, alias, phase/coherence, parameter-recall and sample-rate tests; Windows VST3 CI and pluginval. LA-2A-inspired LIMIT is a leveler, not a brickwall limiter.

## State

0.2.0 development candidate, based on the user-specified current commit
`eaa47f0971ae8044592f9b2b5146c8faf69b1d6d` (`feature/xray-3d-fluid`).
No side JUKE rails. Native 4K output, live GPU submission fix, reflective mercury
surface, FIELD, clockwise compression amount, bipolar CARVE and unload regression
coverage. See [engineering notes](docs/SCALPEL_0.2_ENGINEERING.md) for exact behavior,
verification scope, migration and performance limits.

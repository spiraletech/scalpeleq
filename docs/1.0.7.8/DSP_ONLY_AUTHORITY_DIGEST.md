# Scalpel EQ 1.0.7.8 — DSP-only authority digest

Date: 2026-11-16 (America/Los_Angeles)

## Source authority

- Repository: `spiraletech/scalpeleq`
- Protected branch: `ui-fidelity/band-anchored-carve-1.0.7.7`
- Exact protected baseline: `ad546dcc1c829a83d894d7f8fe94b55c5c131d79`
- Isolated patch branch: `fix/scalpel-1.0.7.8-dsp-only`
- Target internal version: `1.0.7.8`
- Plugin identity remains `com.spiraletech.scalpeleq`, manufacturer `SpEt`,
  plugin code `ScE1`; historical parameter IDs and state layout are unchanged.

The attached Engineering Reference and User Manual were reviewed as draft
specifications. Their protected 1.0.7.7 visual description and honest
verification language remain authoritative. Their proposed lifecycle,
add/remove-band and magnetic-relationship UI sections are design targets, not
verified implementation, and are explicitly superseded for this patch by the
user's later DSP-only correction.

## Protected 1.0.7.7 interface

This branch must retain exactly:

- all seven established BAND 1–7 controls and fourteen graph boundaries;
- the approved champagne-ivory chassis, graphite hardware, screws, type,
  glass, proportions, control dimensions and spacing;
- the BAND / CARVE / COLOR / ANALYZE workspaces;
- existing graph hit-testing, selection and analyzer presentation;
- existing preset and automation identities.

No band-presence state, add/remove interaction, magnetic-link state, new
controls, material pass, analyzer redesign or rejected 1.0.7.8 visual
experiment is authorized here.

## Authorized internal change

Replace the narrow cascaded-notch BAND OFF path with a fixed-storage,
high-rejection Butterworth band-stop that suppresses the whole interval
between the existing two band boundaries. Two frozen filter banks crossfade
over 20 ms when boundaries move, keeping recursive state paired with its
coefficients. The audio loop performs no memory allocation or UI traversal.

BAND OFF follows the band's Stereo/Mid/Side domain. MONO remains a
frequency-region SIDE operation. The design reports rather than hides its
transition width: the mathematical boundaries are approximately -3.01 dB,
while the >=60 dB release target applies to the central 50% of logarithmic
band width.

## Verification contract

- sample rates: 44.1, 48, 96 and 192 kHz;
- narrow, medium, wide and 10–20 kHz stopbands;
- >=60 dB rejection throughout the declared interior test region;
- <=0.1 dB measured out-of-band change at valid probes;
- selected-domain removal with the unselected domain unchanged;
- impulse, white-noise, multitone, sweep and live-boundary movement safety;
- finite output, zero processing allocations and faster-than-real-time
  worst-case all-seven removal on the reference CI host;
- inherited DSP/routing/analyzer regressions;
- Windows x64 VST3 build and strictness-8 pluginval before packaging;
- FL Studio 25 project recall, automation, playback, close and removal remain
  a separate physical-host gate and must not be inferred from pluginval.

Passing these gates produces a candidate only. Merge, stable release and
canonical promotion remain reserved for Agnathos.

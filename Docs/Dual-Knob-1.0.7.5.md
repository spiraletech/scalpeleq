# Scalpel EQ 1.0.7.5 — dual knob calibration

Base: verified 1.0.7.4 (`0d551c174c41eb18504ce7ffdd02e91aa64ffc39`). UI geometry, material masters, fonts, analyzer and DSP algorithms are unchanged.

## COMP THRESHOLD

The existing APVTS threshold parameter remains -60..0 dBFS in 0.1dB steps, default 0, with its original ID and normalized host mapping. Only the slider's local virtual travel is reversed: `dB=-60*p`, `p=-dB/60`. The clockwise pointer sweep is 225..495 degrees. The readout includes dBFS. The existing SliderAttachment continues to send raw threshold values and gestures. Mode changes do not reset threshold or activate compression implicitly.

## CARVE tightness

The existing raw-Q parameter remains 0.300..96.000 with its original 0.001 step, skew, ID, automation text and preset representation. The existing logarithmic SliderAttachment remains authoritative. The editable field now shows percentage `100*log(Q/.3)/log(320)` instead of raw Q. Typed percentages are inverted with `Q=.3*pow(320,t)`. The top label already includes `% TIGHT`; the field therefore shows 0.0, 50.0 and 100.0 without duplicating the unit. The tooltip distinguishes percentage from raw-Q automation. The two calibrated sliders explicitly retain SHIFT-wheel fine adjustment (one tenth of normal wheel travel).

| Clockwise travel | Threshold | Tightness field | Internal Q |
|---|---:|---:|---:|
| 0 / bottom-left | 0.0 dBFS | 0.0 | 0.300 |
| .5 / top | -30.0 dBFS | 50.0 | 5.366563 (snapped to 5.367 by existing parameter precision) |
| 1 / bottom-right | -60.0 dBFS | 100.0 | 96.000 |

Existing positive Q bounds, filter coefficient smoothing, threshold smoothing and compressor de-click transitions are retained. COMP OFF becomes dry through the existing transition rather than introducing an abrupt coefficient/state reset.

## Verification

- Standalone measured DSP regression: 44.1/48/96kHz; all five compressor profiles at 0/-30/-60; threshold-independent COMP OFF; progressively narrower CARVE response; center attenuation retained; finite, bounded continuous Q sweep.
- Editor/parameter regression: exact local endpoints/midpoint; unchanged host ranges/skew; actual dropdown/profile-to-audio gain-reduction link; drag, wheel, SHIFT fine; percentage editing; background-thread raw automation; preset save/load and editor reopening.
- Native built-VST3 captures add all six calibration positions. They are actual host screenshots, not generated renders.
- Windows x64 build, existing regressions and pluginval remain required. FL Studio verification is only claimed if that host is actually used.

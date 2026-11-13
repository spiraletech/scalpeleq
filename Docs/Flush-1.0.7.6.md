# Scalpel EQ 1.0.7.6 — flush BAND/CARVE buttons

Base: verified 1.0.7.5, `afa3bfa2d029824517ea91d4db5d4a4da29511fc`.

Scope: BAND MODE and CARVE lower-deck ON/OFF and four-button action/slot grids. Existing knob materials, positions, numeric fields and threshold/Q calibration are preserved. Header, band rail, analyzer, COLOR/ANALYZE layout, deck footprint and all eight screw anchors remain fixed.

The supplied kit defines flush as common hardware dimensions, baseline, material, corner radius, bevel, label padding and status treatment, not identical global X coordinates between different functional sections. Its no-text SVG cap masters are reconstructed through native JUCE vector layers; labels remain live native type and parameter controls remain interactive. No reference crop with baked text/pointer is used as a control sprite.

## Geometry

Reference lower panel: `(32,567,1574,312)` on the supplied 1647x955 canvas. Button centers are registered relative to that panel with one uniform scale based on the actual deck width. The editor is not resized or stretched to fit host chrome.

| Family | Reference face size | Row centers Y | BAND X centers | CARVE X centers |
|---|---|---|---|---|
| ON/OFF | 190x72 | 660,749 | 196 | 214 |
| Action/slot grid | 188x66 | 662,753 | 1270,1480 | 1239,1447 |

ON/OFF share the same graphite face. Selection changes only the green/red status light; it never swaps body material. MONO/SOLO/PHASE CORRECTION/LIMIT and C1-C4 share one ivory cap geometry, with the existing selected amber treatment. All matching labels use native centered baselines without per-page offsets. Parent-cached contact shadows follow the actual new bounds, avoiding old-position shadow remnants and double shadows.

## Preserved behavior

- Threshold: default 0.0 dBFS at bottom-left; clockwise reaches -60.0 dBFS. Compressor modes retain their threshold.
- CARVE tightness: default display 0.0 at bottom-left; logarithmic 0-100 display retains positive raw-Q automation 0.300-96.000 and SHIFT-wheel fine control.
- BAND/CARVE ON/OFF, band selection, all four C slots, action toggles, dropdowns, DSP, IDs and preset serialization remain unchanged.

The reference renders disagree on global analyzer/screw positions. This button-only patch does not move established hardware to reproduce those unrelated screenshot differences. Knob/readout geometry is intentionally not shifted to imitate a full new screen.

## Verification gates

Existing 1.0.7.5 DSP/knob/automation/preset tests remain active. Added checks compare actual BAND/CARVE component bounds to the supplied centers/sizes, enforce identical button family dimensions, and compare same-state cap rasterization across pages. Native VST3 capture uses the new actual hit centers for CARVE ON/OFF. Source-lock audit, successful Windows x64 build, all regressions, native button inspection and pluginval remain required.

No FL Studio-specific pass is claimed unless that host is actually used. No automatic stable promotion.

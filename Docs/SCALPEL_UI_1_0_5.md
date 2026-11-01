# Scalpel EQ 1.0.5 UI source of truth

The four approved 1536 × 864 console renders define one interface, not four unrelated skins. The implementation uses a single 1120 × 630 logical grid and JUCE's uniform host transform.

## Non-negotiable hierarchy

1. Vented champagne molded chassis.
2. Black identity plaque: **SCALPEL EQ / PRECISION EQUALIZER / ANALOG CONSOLE SERIES**.
3. Shared analyzer lens with X-RAY, a real peak-hold readout, RESET, and measured L/R meters.
4. Four equal page tabs. The active page is black with one amber underline.
5. One page-specific control shelf with equal cells and no ornamental controls.

BAND MODE and CARVE are the approved typographic reference for all pages. COLOR and ANALYZE inherit their label weight, italic tab treatment, spacing, black value windows, and amber active-state language.

## Page contract

| Page | Controls |
|---|---|
| BAND MODE | ON/OFF, BAND GAIN, COMP THRESHOLD, COMPRESSION, EQ MODE, MONO, SOLO, PHASE CORRECTION, LIMIT |
| CARVE | selected BAND/CARVE label, ON/OFF, FREQUENCY, CUT DEPTH, Q/TIGHTNESS, C1–C4 |
| COLOR | DYNAMIC, SPARKLE, SHINE, SMOOTHENING, VINTAGE TAPE |
| ANALYZE | HEAT MAP, FREEZE/RESUME, VISUAL DECAY |

PHASE CORRECTION remains the correct product language even where an early concept render abbreviated it to PHASE.

## Band identity

| Band | Color |
|---|---|
| 1 | Red |
| 2 | Orange |
| 3 | Green |
| 4 | Cyan |
| 5 | Dark blue |
| 6 | Amethyst purple |
| 7 | Pink |

The band handles are the band selector. The old row of seven rectangular BAND buttons is not part of 1.0.5.

## Asset rules

- Chassis, plaque, panels, tabs, controls, meters, and knobs are vector/software rendered.
- No full-window raster background and no GPU requirement.
- Black knobs use a measured amber value arc and a white pointer.
- L/R meters show independent post-processing channel peaks.
- RESET clears the real peak-hold state.
- Host resizing remains available; no bottom-right JUCE resize tab is drawn.
- Existing APVTS IDs, DSP, presets, phase correction, CARVE behavior, and overlap hit-testing remain unchanged.

# Scalpel EQ 1.0.8.7 — BAND MODE Glass and Dial Contract

This is the first lower-screen menu pass. It applies to BAND MODE only.

## Glass substrate

- The lower BAND MODE screen is one transparent-black OLED laminate.
- Its black pixels retain alpha and composite over the existing chassis.
- It contains no baked controls, labels, values, tan cards, dividers, or LEDs.
- Existing housing, screws, screen bounds, touch targets, and control geometry remain locked.

Production asset: `f1087_band_mode_glass.png` (1570 × 317 RGBA).

## Dial scales

Production asset: `f1087_band_dial_rings.png` (608 × 304 RGBA).

| Cell | Control | Range | Divisions | Zero position |
| --- | --- | ---: | ---: | --- |
| 0 | BAND GAIN | −60 to +60 dB | 49 | 12 o’clock |
| 1 | COMP THRESHOLD | −9 to +9 dB | 37 | 12 o’clock |

Both controls use the same 270° JUCE rotary travel:

- minimum: 225°, bottom-left
- zero: 360°, twelve o’clock
- maximum: 495°, bottom-right

The neutral ring is asset-backed. Live red segments and the white pointer use
the identical JUCE-angle coordinate function, so the rendered scale, pointer,
parameter value, automation value, and DSP value cannot disagree.

## Prohibited drift

- No opaque black rectangle inside the lower glass.
- No khaki, champagne, beige, amber, or brown value wells.
- No mismatched tick and pointer angle conventions.
- No movement or resizing of BAND MODE controls.
- No changes to CARVE or ANALYZE in this pass.

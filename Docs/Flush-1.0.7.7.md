# Scalpel EQ 1.0.7.7 — BAND-anchored CARVE layout

Base: verified 1.0.7.6 review commit `2974856028c5b46d67bcb9378cf7acda32342c89`.

This patch changes CARVE geometry only. BAND MODE is the lower-panel authority. CARVE ON/OFF now uses the exact BAND ON/OFF rectangles; C1-C4 use the exact MONO/SOLO/PHASE CORRECTION/LIMIT rectangles. The three existing CARVE sliders retain their size, knob material, live pointer, text boxes, parameter mappings and calibration, while their centres are registered to screenshot-space X positions 371, 598 and 826 through the locked BAND panel `(31,486,1276,265)`. CARVE separators use screenshot-space X positions 256, 484, 712 and 940 through the same panel registration.

No chassis, palette, material, screw, header, analyzer, band, DSP, automation, preset or editor-size changes are included. Static reference crops are not used as interactive controls.

Verification requires the existing regression suite, new shared-rectangle and target-centre gates, Windows x64 VST3 build, native BAND/CARVE captures, and pluginval. FL Studio verification is reported only if FL Studio is actually used. No stable promotion is performed.

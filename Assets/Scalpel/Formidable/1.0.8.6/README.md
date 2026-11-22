# Scalpel EQ Formidable 1.0.8.6 runtime assets

This directory is the compiled subset of the approved detailed black/red asset
handoff.  It intentionally excludes reference crops containing labels, meters,
spectra, knob values, or DSP state.  Those elements remain native JUCE content.

## Compiled surfaces

- `f1086_lacquer_shell.png` — shared automotive-black chassis.
- `f1086_graph_glass.png` — neutral analyzer lens reflection.
- `f1086_control_glass.png` — lower laminated OLED/touch surface.
- `f1086_band_*` — idle, hover, pressed, and selected hardware key states.
- `f1086_tab_*` — idle, hover, pressed, and selected mode key states.
- `f1086_heat_map_flame.png` and `f1086_freeze_snowflake.png` — white OLED glyphs.
- `f1086_chevron_*` — carousel navigation glyphs.
- `f1086_dial_pointer.png` — high-DPI white dial indicator.

The state atlases are retained as visual regression references but are not
embedded in the plugin.  SVG glyph sources are retained for deterministic
future raster scales.

## Runtime rules

1. Graph data, axes, nodes, held peaks, meters, values, and automation state are
   never baked into material assets.
2. The graph glass and lower OLED glass remain independent optical layers.
3. Red belongs to selected physical hardware and live measurement energy.
4. Lower-screen selected content is white; inactive content is graphite.
5. Compression and Ratio are bounded carousels—no popup menu is opened.
6. Heat Map and Freeze are glyph-only touch controls with no plastic button,
   underline, blue edge, or warm selected lamp.


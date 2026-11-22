# Scalpel EQ 1.0.8.6 — black/red functional UI patch

## Scope

1.0.8.6 promotes the approved detailed black/red asset handoff into actual
compiled JUCE resources.  The patch changes presentation and selector
interaction only.  DSP algorithms, APVTS parameter IDs, parameter ranges,
preset serialization, graph geometry, band geometry, and editor dimensions are
preserved from the 1.0.8.5 baseline.

## Runtime mapping

| UI region | Runtime implementation |
| --- | --- |
| Chassis | 1670×942 black lacquer master mapped to the locked enclosure |
| Analyzer | Existing live graph followed by independent neutral glass overlay |
| Lower deck | Seamless OLED glass; controls remain native and page-dependent |
| Band 1–7 | Four physical key states, selected hardware carrying restrained red |
| Band/Carve/Analyze | Four physical mode-key states |
| Band and Carve dials | Native calibrated arcs with supplied high-DPI pointer |
| Compression/Ratio | White selected value, graphite label, supplied chevrons |
| Heat Map/Freeze | Supplied white glyphs, opacity-only state, no button card |

## Carousel interaction

Compression and Ratio retain their existing ComboBox/APVTS attachments but no
longer open a popup menu.  Selection is bounded and can be changed by:

- left/right chevron click;
- horizontal swipe or mouse drag;
- mouse wheel or horizontal trackpad wheel;
- arrow keys.

Every path changes the same attached selection.  Host automation and preset
recall remain authoritative.

## Release gates

- CMake and binary release version must equal `1.0.8.6`.
- Every compiled PNG must decode and match its manifest dimensions.
- The portable DSP suite must remain unchanged and pass.
- Windows x64 must compile the VST3 and test executable.
- UI regression captures must cover Band, Carve, and Analyze at 1344×756 and
  1792×1008.
- The carousel regression must preserve the original parameter after a
  right/left round trip.
- `pluginval` strictness 8 must report `SUCCESS` before release promotion.


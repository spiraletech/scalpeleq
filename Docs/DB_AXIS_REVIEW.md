# Analyzer dB axis correction

Base: `4c9637914081529e52621eed6d4c41f0b8774cc7`, the verified ivory review build.
Reference: supplied `Scalpel_EQ_dB_Axis_Formidable_Assets(1).zip`, geometry/tokens
JSON and native 1344×756 proof. Only the analyzer's right-hand label gutter is
restyled. No default/stable branch promotion is authorized by this change.

The live display calibration was already linear and correct. Previously the
gutter lacked a unit cap, left-aligned numerals, and crowded the 20kHz node.

The new native axis follows the supplied foreground asset: `dB` unit cap,
0/−5/−10/−15/−20/−25/−30/−35/−40/−50/−60, a shared right edge, warm neutral
text on #040708. It uses native Arial (the asset's documented sans-serif
fallback), not rasterized labels or the full-screen proof as artwork.

Geometry follows actual plot bounds. At native 1344×756 the gutter begins
6px beyond the plot right, its labels end 34px beyond the plot right, and
the unit cap sits 17px above the 0dB row. Sizes are 11px labels and 9px unit
at native scale. The existing uniform editor transform scales these values.
Actual plot top/bottom are retained rather than moved to screenshot estimates
(the kit estimates top183/bottom387). This keeps labels calibrated at every
resize/DPI and avoids moving the plot, frequency labels, or band nodes.

One shared formula controls labels, grid and trace: `y=top−dB×height/60`.
The existing analyzer slope/pivot remains in `surgery::displayDb`; spectrum
labels describe the displayed level. PEAK(dBFS) remains an independent,
unchanged audio measurement. No synthetic tick marks or extra borders appear.

Preserved: DSP, preset/automation IDs, routing, plot dimensions, selected
band/CARVE state, graph handles, X-RAY, RESET, peak/meter module, all eight
screws, chassis/menus/buttons, and static-frame caching.

Changed implementation: `ScalpelDbScale.h`, `ScalpelDbAxis.h`, targeted calls
in `RootsEditor.cpp`. CMake adds `ScalpelDbAxisTests.cpp`: independent expected
native rows, midpoint/−35 uniqueness, all intermediate levels at six scales,
known amplitude ratios, presentation tilt, and floor/top clipping. Existing
FFT calibration and regression suites remain active.

Local portable C++20 axis calibration: PASS. Windows build, full regression,
pluginval and actual VST3 screenshots must pass before packaging. FL Studio
is not installed here; no FL Studio verification is claimed.

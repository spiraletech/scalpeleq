# ANALYZE deck fill — 1.0.7.3 review
Parent: 0d34ab91ae77a7c06945d42e804b2e464cc08d40 (1.0.7.2).
Branch: ui-fidelity/analyze-fill-1.0.7.3. No stable promotion.

The two supplied Formidable sheets are implementation references, not production sprites.
The blueprint explicitly locks the lower panel footprint, outer screws and three-column
layout. The red marked area is inside that panel. This patch reduces INTERNAL empty space;
the separate plain shell strip below the panel remains unchanged under that geometry lock.
It does not resize/crop the editor or move its panel/screw geometry.

Locked authored deck: x34 y578 width1588 height258, in the existing 1670x942 source
coordinate system. All eight screw centers, analyzer housing and plot mapping, band rail,
header and other page-control bounds are unchanged.

ANALYZE changes:
- HEAT MAP and FREEZE canvases: 120x120 -> 196x196 source units, at x192/1272,
  y626. Their x centers remain 290 and 1370, inside their established columns.
  Opaque body height increases from approximately 63 to 114 logical pixels.
  Expected lower-body clearance reduces from approximately 54 to 18 logical pixels
  (about 65px -> 22px at the 1344x756 native client). Actual values are captured in CSV.
- VISUAL DECAY: centered at x835 as before; bounds x733,y622,w204,h207.
  Readout 124x26 logical pixels, with a wider field, a larger rotary face and preserved
  heading separation. No ticks, illuminated pointer or new controls.
- The existing flame is scaled as a vector master, preserving its aspect and gesture;
  the frost glyph remains a vector. HEAT MAP's broad orange halo is restrained to a
  warm hardware edge, retaining the existing functional amber flame.
- Selected tabs use a consistent warm amber label and filled amber underline, including
  ANALYZE. Selection still uses mode_ and actual button toggle state. No parallel state.
- Live decay values are preserved; the static reference's 0.05 is not forced onto presets.

Verification: existing full CTest suite, Windows x64 VST3 build, pluginval and actual
native VST3 page captures. Added assertions require enlarged opaque icon bodies, limited
bottom clearance, panel containment, heading clearance and unchanged editor dimensions.
No DSP, audio latency, IDs, automation, state serialization or parameter defaults change.
No FL Studio 25 verification is claimed unless actually performed.

# Scalpel EQ 1.0.8.7 — ANALYZE B COMMAND

## Outcome

The ANALYZE page now uses the approved B — Command icon scale. Heat Map and Freeze are matched OLED touch symbols with a measured 56 px visible envelope at the 1344 x 789 reference UI size.

## Corrected failure

The previous 4x PNG masters contained excessive transparent padding. Runtime sizing therefore enlarged the canvas rather than the visible mark, leaving both icons visually timid beside Visual Decay.

## Implementation

- Re-authored both symbols on tight 64 x 64 logical transparent canvases.
- Retained 256 x 256 4x runtime masters for clean downsampling.
- Locked the optical stroke near 2.5 px.
- Draws the asset canvas at 64 logical px, yielding a 56 px visible mark.
- Preserved white selected state and graphite idle/hover states using opacity only.
- Added a regression envelope of 53–59 px with a maximum 3 px centering error.

## Protected scope

No DSP, analyzer measurement, Freeze behavior, Heat Map behavior, Visual Decay behavior, graph geometry, panel glass, tabs, captions, or other page layouts were changed.

# Approved Scalpel EQ visual assets

- `scalpel-knob.png`: transparent, powder-textured ivory dial with molded V pointer. Same source art for all five COLOR knobs; rotate the entire dial with the actual parameter value. **Never paint perimeter notch/tick marks.**
- `scalpel-freeze.png`: transparent sculpted ivory snowflake pushbutton.
- `scalpel-heatmap.png`: transparent sculpted ivory flame pushbutton.
- `ui-approved-reference.png`: approved October 24, 2026, layout/finish reference with warm vintage voicemail-machine ivory, seven BAND tabs, black recessed Phillips screws, clean COLOR panel, and mercury X-RAY preview.

**Important discrepancy:** the reference screenshot still labels its right-hand vertical dBFS axis `0 -12 -24 ... -96`. The final Scalpel EQ graph **must** instead label `0 -5 -10 -15 -20 -25 -30 -35 -40 -50 -60`, while preserving the independent internal analyzer precision. Image text is not the source of truth for scale mathematics.

Asset source is the user-provided design references. Do not silently regenerate or substitute their knob/buttons; small GUI performance-optimized derivatives may be generated with visual parity and lossless source retention.

## 0.2.0 additional supplied master

`scalpel-screw.png` is the user's transparent 1254x1254 recessed Phillips screw.
It is embedded without regenerating or recoloring the original master.
All master PNGs are retained at their supplied resolution. Native 4K UI rendering
does not turn lower-resolution reference screenshots into native 4K source art.
The chassis and display overlays are drawn with scalable geometry.

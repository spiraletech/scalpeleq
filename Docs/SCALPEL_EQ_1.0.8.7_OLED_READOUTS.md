# Scalpel EQ 1.0.8.7 — OLED Readout Asset Contract

## Locked visual rule

The lower BAND, CARVE, and ANALYZE panel is one continuous black glass display.
Numeric values are light emitted by that display, not separate inset LCD cards.

- White-neutral seven-segment numerals are rendered directly on glass.
- Units remain compact neutral-white typography beside the digits.
- No beige, tan, champagne, khaki, amber, outline, bezel, fill, or glow is permitted behind a value.
- Existing dial, label, hit-target, automation, panel, and graph geometry is unchanged.
- Editable slider labels retain their existing bounds and host interaction.

## Production asset

`Assets/Scalpel/Formidable/1.0.8.7/f1087_oled_digits.png`

- Canvas: 624 × 72 transparent PNG
- Cells: 13 × 48 × 72
- Cell order: `0 1 2 3 4 5 6 7 8 9 - . +`
- Light colour: `#F2F5F7`
- Background: fully transparent
- Source master: `source/f1087_oled_digits.svg`

## Rendering contract

`scalpel::finish::oledNumber()` centres the numeric and unit string inside the
existing JUCE slider text box. It scales down only when necessary and never
paints a background. `SurgeryLookAndFeel::drawLabel()` routes every finished
slider readout through this renderer.

The former `f1081_readout_idle.png` card has been removed from BinaryData and
from the finished slider-label rendering path.

## Regression gate

The formidable finish test verifies:

1. The embedded atlas is valid and exactly 624 × 72.
2. A real finished slider label produces visible digital ink.
3. No khaki-coloured opaque pixels appear.
4. The readout corners remain transparent, proving no hidden card is painted.

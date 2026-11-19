# Scalpel EQ 1.0.8.4.2 analyzer integration

Use `asset_manifest.json` to map the 2172 × 436 design canvas to the analyzer component. The full faceplate and bezel each draw to the component bounds. Position the other parts using their normalized rectangles. Keep the aspect ratio of the screws, lamps, node rings, and meter segments. The control atlas and standalone PNGs have matching pixels.

The atlas contains straight-alpha PNG sprites with padding between items. Draw the renderer's text over the control states and map the pressed state to the pointer-down interaction. Draw separate lamp and meter states from the existing plugin data.

## Graph and numbers

- Draw one vertical column titled **dBFS** on the right. Use **0, −10, −20, −30, −40, −50, −60** from top to bottom. Use the same coordinate function for its gridlines and the measured spectrum.
- Draw the selected band's signed **BAND EQ** value inside the upper-right plot area. Keep its response contour and gain scale math independent from the dBFS ruler. Use the existing response scale to plot the signed curve and identify its value in the readout or under the pointer.
- Draw frequency labels from the shared log-frequency transform. A 20 Hz label belongs at the plot's start and 20 kHz at its end. Preserve the selected band's actual frequencies and existing magnet hit targets.
- Drive peak and compressor readouts from the existing graph state. At silence, display the plugin's empty reading or floor through the native control.

## Audio-lit polycarbonate

1. Composite the faceplate, bezel, smoked substrate, and gasket. Apply the low-opacity pixel mask inside the plot.
2. Use one interpolated spectrum frame for both the crisp trace and its emitted light. Per column, draw a narrow blue rim at the crest and extend a faint emission below it using `spectrum_falloff.png`. The file is a normalized vertical opacity ramp. Tint it toward pale steel blue, with a restrained ice-blue accent on strong crests.
3. Set intensity from the same displayed signal values. A suggested starting curve is `pow(clamp((dbfs + 60) / 60, 0, 1), 1.6)`. At the display floor the contribution is zero. Apply the plot clip to the fill and any short-radius crest bloom.
4. Composite the lens reflection over the light pass. Draw live controls and numbers above reflection and signal layers. Keep the bright crest thin and fade the emission quickly below it, so a quiet slice of the plot stays nearly black.
5. Feed Freeze and Visual Decay into the existing analyzer frame. Derive the glow from that frame so emission and trace use the same timing. The asset kit contains RGBA plates and example frames. Runtime audio animation is implemented in the plugin renderer.

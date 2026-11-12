# Formidable 1.0.7.1 review contract

Verified functional parent: `855e4a81bf3bd3f5fdede5e2a8066543f8f7ba32`.
Preserved Windows binary: Actions run 43030365148, VST3 artifact 13860124298,
ZIP SHA256 d6ee0e9f83871c4ef02d98a29a17efaf9a616b9709b55c69022d542b972e37a8.
Separate branch: `ui-fidelity/formidable-1.0.7.1`. No default branch promotion.
Version 1.0.7.1 identifies this user-requested development candidate.

## Source and coordinate contract

The new kit replaces the previous 1645×956 reference. Its 1670×942 image has
no FL title bar. Uniform source scale is 630/942, horizontal centering in the
1120×630 logical editor; the existing 16:9 editor transform handles resizing
and DPI. Default client stays 1344×756 across all pages.

| Component | Source x,y,w,h | Owner |
| --- | --- | --- |
| Band rail | 45,104,1564,92 | cached rack |
| Seven equal keys | 65+221i,116,214,68 | reference::band / hardware |
| Analyzer full housing | 41,200,1581,298 | RootsGraph static cache |
| Measured plot glass | 136,243,1077,207 | reference::plotLocal |
| Lower deck | 34,578,1588,258 | cached rack |
| ON / OFF | 109,623/709,181,74 | same darkButton master |
| BAND gain / threshold | 352/592,624,164/166,199 | knob + live value |
| CARVE knobs | centers 460/705/995,624,166,199 | same master |
| COLOR knobs | 137+310i,624,166,199 | same master |
| ANALYZE decay | 752,624,166,199 | same master |

Crop boxes include shadows and labels, and are not automatically hitboxes.
Equal band key widths normalize the manifest's unequal crop padding. The plot
is measured inside PANEL_005, whose crop also contains bezel and axis content.
All eleven dB ticks use the existing shared calibrated 0…−60 transform. The
reference's uneven text rows are not allowed to distort measured FFT values.
The PEAK display is still dBFS. Original audio-driven traces, stored band colors
and functional boundary/Carve handles are retained.

Exactly eight native screws use the kit's individual centers: analyzer
(72,231), (1600,232), (72,478), (1598,478); lower (69,614), (1607,618),
(69,810), (1605,812). No internal plot fasteners.

The rejected footer plaque and perforations are omitted, leaving only the
existing shell material in that region. No replacement badge or ornament.
Header uses upright native type, SURGICAL EQUALIZER / ETHERTECH, with no added
spiral or decorative dividers absent from the new reference.

## Material and state implementation

Warm titanium-polymer palette uses the kit's B6A493/CCB7A5/786C60 shell,
BFAE9E hardware, D5B18A selected key, 292522 graphite and E6D4BC dark-face
lettering. Native text is upright Segoe UI at defined role sizes and regular
weight; the main wordmark is bold. Numerical fields retain tabular advances.

One embedded, lettering-free material sample supplies cached mirrored relief.
Each material retains separate light distribution: broad shell gradients,
multi-stop key faces and bevels, recessed graphite tabs/dropdowns, near-black
glass, and concentric software-cached knob material with vector grip/rim.
White F0EBE1 pointers have no halo or illuminated arcs.

Both ON/OFF buttons keep the same body and type in every state. Only the
mutually exclusive 88DB32 green / E94A45 red strip follows the actual parameter.
X-RAY retains its independent red lamp. One filled warm selected-tab underline
follows mode_; no extra track. Native click coordinates in the capture harness
now derive from the same reference geometry as the actual controls.

## Required evidence

Windows x64 build, all existing CTests, strictness-8 pluginval, actual VST3 native
window captures for all four pages and BAND/CARVE ON/OFF, two-scale geometry,
state/parameter preservation, calibrated analyzer tests and cache/performance
measurements. Compare native client pixels to the source with forbidden footer
regions and intentionally upright branding excluded from diagnostic metrics.

Comparable audio fixture: 48kHz, 512 samples, 80 blocks of seed-1071645 lowpassed
noise + 110Hz sine, right channel 0.85× left. Silent BAND1/ON is the reference
comparison fixture. Other pages preserve BAND3/C4, 215.87Hz, 0 cut, Q0.300.

Build and visual results are pending until evidence is collected. FL Studio 25
is unavailable here; no FL playback/project/close/removal test may be claimed.
No 1:1 visual match or release approval is implied by a successful compilation.

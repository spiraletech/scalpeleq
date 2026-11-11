# Ivory/graphite skin review — proposed 1.0.7

Functional base: `81390ced70814044e2cff97c0c2b1b7c5638d31d`. Separate branch:
`ui-fidelity/ivory-1.0.7-review`. Binary compatibility version remains 1.0.6;
this is a skin candidate, not promotion of a stable release.

## Reference contract

Asset kit reference SHA-256:
`a1e576c43472a6433da6f6e123dbc61de4ead8452feb233e967ee4e8744e5c80`.
The original full image is 1645 × 956. Its first 35 pixels are FL host chrome.
Use uniform scale `1120/1645`; center the remaining 921px in the 630px logical
client height (1.47 logical pixels of vertical compensation). The existing
16:9 resize/DPI transform and 1344 × 756 default client are preserved.

The supplied manifest boxes are padded crops, not exact control bounds.
Visible edges in the full image define the measured rectangles below.

| Element | Full reference x,y,w,h | Renderer |
| --- | --- | --- |
| Header | 20,41,1605,88 | ScalpelUI105 / finish::panel |
| Full analyzer housing | 33,216,1583,346 | RootsGraph / finish::analyzerHousing |
| Plot | 122,258,1114,248 | RootsGraph / reference::plotLocal |
| Lower plate | 25,646,1593,256 | ScalpelUI105 / finish::panel |
| ON / OFF | 111,699/783,173,64 | LookAndFeel / finish::darkButton |
| Gain control + readout | 342,689,164,188 | LookAndFeel / finish::knob / number |
| Threshold + readout | 579,689,166,188 | Same masters |
| COMP / DOMAIN | 814,705/795,322,50/51 | LookAndFeel / darkButton |
| MONO/SOLO | 1198/1403,688,187/185,69 | LookAndFeel / hardware |
| PHASE/LIMIT | 1198/1403,779,187/185,69 | Same master |

Band and tab rectangles are individually measured in `ScalpelReferenceLayout.h`.
All other pages share these material masters; their existing controls and order
are retained within the new lower plate. The kit supplies an ivory BAND render,
not individually approved ivory CARVE/COLOR/ANALYZE renders.

## Materials and state rules

- Ivory ABS: EBD7C7 base, F0DECE highlight, C7B8AB shade; cached deterministic
  micrograin and sparse low-contrast surface scuffs. No global metallic glaze.
- Graphite keys: 303131 midtone, recessed contact shadows, restrained gray edge.
- Both ON/OFF bodies stay identical. Only the selected strip lights: ON 8CD848,
  OFF EF4845. The reference picture lights both; the kit's explicit mutually
  exclusive state rule takes precedence over that static illustration.
- Knob body 252627; radial grip teeth and concentric relief; F4F4F2 printed
  pointer with no LED halo or external illuminated arc.
  The 512px software-backed face material is generated once; pointer, grip and
  bezel stay vector geometry. No reference crops are pasted into controls.
- Header uses upright native Segoe UI, SURGICAL EQUALIZER and ETHERTECH. The
  original small spiral is a vector contour traced solely from the provided mark.
- Analyzer 080C0E; red X-RAY lamp; live peak/meter/FFT paths preserved. Existing
  stored band colors and boundary handles remain authoritative.
- Exactly eight cross screws from one primitive: four per outer housing.
  Inset in reference units: analyzer 32×28; lower plate 29×28. These are attached
  to the full panel corners, beyond the meter housing, never to plot bounds.
- Selected tab has one complete warm-white underline. No gray secondary track.

## Verification gates

First inspect native compiled BAND against the normalized reference (side-by-side,
50% overlay, difference). Correct geometry/material defects before accepting the
shared CARVE/COLOR/ANALYZE appearance. Pixel metrics exclude live analyzer content
and state-dependent lights. The two command states are captured on BAND/CARVE.

Existing regression coverage includes host parameter/preset compatibility, seven
bands, all slots, smooth Hz/Q bindings, graph synchronization during frozen
silence, audio allocation checks, editor lifecycle, all pages and size/DPI caches.
Native screenshot harness loads the built Windows VST3 into a JUCE host, captures
the HWND using BitBlt and exercises actual mouse clicks. Additional seeded audio:
48kHz / 512 samples, 80 blocks, lowpassed seed-1071645 noise plus a 110Hz sine,
right channel 0.85× left. Silence is the primary image-comparison fixture.

FL Studio 25 is not installed in this execution environment. No FL load, project
recall, playback or removal verification is claimed. Build, validation and visual
results must be attached after CI completes; this specification is not evidence
that the candidate has passed.

## First compiled comparison

Windows run `42996835600`, source `a95b6fadd620ce839f020bf7f4fd218a73c8cc43`:
27/27 tests and pluginval strictness 8 passed. Actual native VST3 captures,
not mockups, were compared at 1344×756 to the uniformly normalized reference.
Representative plate medians were already close (header target 234/214/198,
actual 230/210/193; lower plate target 228/208/191, actual 226/207/190).
Remaining observed defects: small control type, clipped contact shadows,
excess ivory below the lower plate, missing silent X-RAY grid, and uniformly
shaded knob centers. The next scoped correction addresses those measured
defects before accepting the remaining shared pages.

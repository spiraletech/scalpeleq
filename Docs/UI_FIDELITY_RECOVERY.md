# UI fidelity recovery: COLOR gate

## Baseline and preservation

Remote source baseline: `4ebc8c678c30623665326d3636eb44a6f2c6f0a9`.
Local checkout HEAD: `3d224552469b45e5b5fabddd50cb84e01a7a401b`; dirty files contain the patches published through GitHub's Git-data API. HEAD alone does not identify that working tree.
Dedicated branch: `ui-fidelity/color-recovery-1.0.6`, created from the remote baseline.
Last complete validated Windows artifact: source `e783df6dff3eb752afe3f92995159516389b8f9a`, run 42573176998.
ZIP SHA256: `81608d6a1974e6984bf5990ffe02d0d57992c870f886748cc00d56d5a0530e7e`.
Latest source passed 26 tests twice; pluginval did not execute because its download returned HTTP 504. This is not validation success.
The preserved package has not been exercised in FL Studio here. Functional baseline means automated checks passed, not host certification.

## Rendering ownership

| Responsibility | Owner |
|---|---|
| Uniform aspect/DPI transform | OnyxRackSkin.h::contentTransform |
| Child bounds, page visibility, attachments | RootsEditor.cpp::layoutRack / setMode / bindNode / bindCarve |
| Static chassis cache | RootsEditor.cpp::paint / paintRack |
| Header, panel geometry, separators | ScalpelUI105.h::drawRackFace |
| Hardware material and cached texture | ScalpelMaterials.h |
| Knobs, text, switch state, dropdown, navigation | SurgeryLookAndFeel.h |
| Live spectrum, boundary handles, CARVE nodes, response traces | RootsGraph::paint, RootsEditor.cpp |
| Monitoring readout and stereo meter rendering | ScalpelUI105.h::drawAnalyzerPanel |
| Native-size screenshots and parameter checks | Tests/QualityTests.cpp |

## Causes of drift verified in source/history

- 9bc676b changed the generic button material to dark without routing BAND selectors and processing buttons through dedicated silver rendering. Dark lettering remained on dark faces.
- Earlier enable correction combined a manual onClick command with ButtonAttachment on the same control. The later e783df6 lineage removes that competing attachment. Preserve this functional fix.
- Fixed 1120-space panel constants were never measured against the 1536 COLOR reference. Scaling is uniform, but the authored positions are wrong: deck y449 * 1.2 = 538.8 versus reference y565 * .875 = 494.375.
- Different drawing paths painted button materials and labels independently. The audit baseline consolidates silver hardware but still has mismatched component proportions.
- Quality screenshots use mutated audio/parameter fixtures, with all four CARVEs active. Their additional circles are enabled CARVE nodes, not fabricated decorations. Static reference comparisons must mask live lens content.
- No aligned reference gate existed. Compilation and broad rendering tests could pass while material and geometry fidelity remained poor.

## Locked references

Files are identified by content hash, not mutable names. All four cover the entire visible faceplate, no host chrome.

| Page | Source filename | SHA256 |
|---|---|---|
| BAND | DD2BC366-786B-4512-B28E-ED7DD71B4F4C(1).jpeg | edd04bc4bd1676a2fcdf38ed55da05fa0dc31fb5b9ab8414dfea7612dc58dbec |
| CARVE | 157D9549-88EF-4661-9067-A55D890CAD19.jpeg | d0a4c7619212725b0e1ff59aae692f1c89338c69ff7170955a796cc91388a714 |
| COLOR | 83B7E556-BA48-4006-A708-7768895BF6D5.jpeg | 8d7e9de0d37b4253dca5d0479af2eef6a1223b5aefd27e9c75f883877fa48297 |
| ANALYZE | 891E4011-912C-49E3-A9AA-D9AFB029A84D.jpeg | 797b3e7210dd44889b17073f1e3664f4d4808e2f4034addb5f2649b9fe8cabac |

Conflict resolutions from approved instructions: upright logo; no slashes; upright shared labels; identical ON/OFF body materials with inset active lamp; preserve real band identities and live metering. Other pages are deferred until COLOR review.

## Reference geometry: COLOR only

Coordinates below are hand-measured visible edges in the 1536x864 image (approximately +/-2 px), not inferred component hitboxes. Conversion to capture is exactly .875. Conversion to retained JUCE logical space is 1120/1536; then the existing uniform host/DPI transform applies. No per-window offsets.

| Component | Reference x,y,w,h | Radius / edge | Owner |
|---|---|---|---|
| Header | 24,16,1488,78 | 22 / 1.5 | drawRackFace |
| Selector housing | 26,104,1484,56 | 10 / 1.5 | drawRackFace |
| BAND faces, step 212 | 31,111,199,42 | 5 / 1 | hardware |
| Analyzer housing | 24,170,1488,315 | 20 / 3 | RootsGraph |
| Plot cavity | 98,187,1044,253 | 0 / 1 | RootsGraph::area |
| Tabs, step 368 | 38,496,366,51 | 7 / 1 | modeButtons |
| Lower deck | 26,565,1485,260 | 24 / 1.5 | drawRackFace |
| Knob centers | 187,479,768,1055,1341; y678 | diameter 125 +/-3 | rotary |
| Value displays | centers as knobs; y749,w138,h34 | 5 / 1 | slider labels |
| Headings | centers as knobs; y584,h25 | none | onyxLabels |
| Deck separators | x338,626,914,1205; y593..793 | 1.5 | drawRackFace |

## Master materials, typography and state contract

| Family | Material / lighting | Font at reference width | States |
|---|---|---|---|
| Chassis/deck | #111317 / #0E0F12; restrained diagonal grain, local graphite highlights | n/a | static; cached per DPI |
| Silver hardware | #E2E4E6 top, #C3C7CB middle, #858D94 bottom; dark contact shadow | bold 20px, #080D12 | same body when selected; inset cyan edge; disabled muted text; hover small luminance change |
| Navigation | recessed graphite, no silver fill | bold 23px; #F1F3F5 selected / #A5ADB6 idle | only mode_ drives selected underline; immediate switch |
| Rotary | near-black graphite, concentric machining, silver rim | values bold 22px, ice/white | pointer follows parameter; hover no material replacement; disabled legible |
| Captions | #F1F3F5 | upright bold 18px | no state-dependent font changes |
| Header | near-black polymer, subdued titanium edging | wordmark bold 42px; tracked captions 17px | static |
| Dropdown | recessed graphite, silver bevel, white arrow | 21px | focus/hover edge; preserve item choices |
| Switches | identical dark graphite body; inset cyan lamp | bold 21px | parameter state drives lamp and edge; never body swap |
| Analyzer | #030D19 cavity; cyan exterior frame only | axes 16px | audio-driven; X-RAY red lamp; real stereo/peak values |

This iteration changes COLOR geometry only. Material reconstruction beyond its baseline remains an open gate; these tokens are constraints, not evidence of a match.

## Verification and promotion gates

- Produce full-bound 1344x756 BEFORE, reference, ACTUAL, 50% overlay, absolute RGB difference. No registration warp, crop, glow, or post-processing of ACTUAL.
- Mask plot and live telemetry when interpreting pixel metrics. Report those separately. Do not treat absent live spectral content as a material defect.
- Panel edges +/-4 capture px; text anchors +/-3 px; control dimensions +/-5%. Values are diagnostics with hand-measurement uncertainty.
- Keep a fresh-instance silent COLOR screenshot to avoid the prior test fixture's mutated parameters, plus the existing repeatable active-audio screenshots.
- Build Windows x64; run existing regressions and pluginval. FL Studio open/play/menu/project-recall/close/removal require actual host verification and remain pending here.
- Do not propagate the COLOR layout to BAND/CARVE/ANALYZE until the aligned COLOR comparison is reviewed. No stable promotion.

## Status

- [x] Prior validated artifact and pre-recovery source preserved locally
- [x] Four references identified by hash
- [x] Drawing owners and prior drift causes inventoried
- [ ] COLOR geometry/materials accepted
- [ ] COLOR overlay reviewed
- [ ] Shared correction propagated to other menus
- [ ] Four menus visually accepted
- [ ] FL Studio regression passed

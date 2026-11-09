# COLOR structural correction: review result

Source correction: `0261854216d8d570b200856541568e81b71f817d`.
Built head: `b4b4021b01b5cfae74cb3c742e99904b4fd00e06`.
Windows run: https://github.com/spiraletech/scalpeleq/actions/runs/42585232254
Branch: `ui-fidelity/color-recovery-1.0.6`. No merge or release promotion.

## Scope and files

- `Source/ScalpelReferenceLayout.h`: measured 1536-space COLOR geometry, converted uniformly to existing 1120-space layout.
- `Source/ScalpelUI105.h`: COLOR-specific panel bounds and separators, using the same reference geometry as child layout.
- `Source/RootsEditor.cpp`: COLOR-specific graph, selectors, navigation, controls and plot-cavity bounds. Other page layouts retain baseline positions.
- `Tests/QualityTests.cpp`: fresh silent 1344x756 COLOR capture; deck-position gate.
- `Tests/compare_color_reference.py`: hash-locked reference scaling, full-bound overlay and unamplified difference image; live-region masking for static MAE.
- `.github/workflows/windows-vst3.yml`: enable isolated recovery branch verification. Separate commit from UI changes.
- `Docs/UI_FIDELITY_RECOVERY.md`: baseline, preservation records, ownership, master specification and remaining gates.

## Evidence and measurements

Reference: 1536x864 COLOR JPEG, SHA256 `8d7e9de0d37b4253dca5d0479af2eef6a1223b5aefd27e9c75f883877fa48297`.
Reference is scaled exactly .875, using its whole bounds. ACTUAL is the compiled JUCE editor's `createComponentSnapshot`, captured by the Windows test executable; it is not an FL Studio screenshot or the loaded VST3 host UI. No screenshot repaint, crop, registration warp or glow was applied.

| Feature | Before capture coordinate | Target capture coordinate | Corrected coordinate |
|---|---:|---:|---:|
| Deck top | 538.8 | 494.375 | 494.375 authored; visible edge about 495 |
| Deck height | 204 | 227.5 | 227.5 authored |
| Analyzer housing top | 168 | 148.75 | 148.75 authored; child snapped about 148.8 |
| Tabs top | 469.2 | 434 | about 434.4 after logical snapping |
| Knob center X, channels 1..5 | 138,390,642,894,1146 | 163.625,419.125,672,923.125,1173.375 | within ~1px from authored X after snapping |

These are structural bounds, not measured font ink extents. Text alignment and all control dimensions have NOT passed the final acceptance gates. No false precision is assigned to optical boundaries in the JPEG reference.

Static-region mean absolute RGB error: BEFORE 41.265/255, ACTUAL 28.545/255 (approximately 30.8% lower). Full-frame MAE: 37.191 to 33.199. These are diagnostic only: BEFORE used the older active-audio regression fixture with altered parameters, whereas ACTUAL is fresh and silent. The static mask excludes live plot and telemetry, but remaining state differences still confound attribution. The score is not a fidelity percentage or acceptance proof.

## Comparison interpretation

The 50% overlay demonstrates materially improved panel/knob/value-row alignment. It also exposes unresolved differences:

- Header wordmark baseline and heading sizes differ. The intentional upright/no-slash canon overrides the conflicting COLOR artwork, but the remaining spacing differences are not intentional.
- Monitoring bay bounds, display scale, X-RAY face and typography differ from target. Only its interactive action bounds were aligned in this geometry pass; internal bay rendering still uses baseline proportions.
- Dial center positions are closer; machined rim, optical depth, pointer and font rendering remain below material acceptance.
- Deck/chassis texture and reflection distribution remain too uniform and visible. No new material scheme was introduced in this correction.
- Some axis labels and control captions remain undersized. Global font enlargement was deliberately deferred to a measured follow-up.
- Fresh silent capture has a visible rainbow field despite -inf meters. This is a detected analyzer initialization/presentation artifact requiring separate diagnosis. No analyzer feature or processing was removed to hide it.
- Existing active-fixture circles are enabled CARVE nodes and response traces; their presence is explained by fixture state rather than extra decorative controls.

## Verification

Windows x64 compilation passed. All 26 regression tests passed. pluginval step passed (existing strictness-8, skip-GUI configuration). Package integrity checked with `unzip -t`.
Existing tests include enable commands across seven bands/four CARVE slots. The geometry pass changes no DSP, parameter IDs, automation mappings or preset representation. No functional bug-fix commit is bundled with this correction.
FL Studio 25 load/playback/menu/control/project-recall/close/removal tests were not performed. Prior host crashes are unresolved and not claimed fixed.

## Gate decision

COLOR structural correction is available for review. COLOR material/typography acceptance is still open. Do not propagate this layout to other menus or promote it as finished UI. Inspect the actual, reference, overlay and difference together before authorizing the next scoped correction.

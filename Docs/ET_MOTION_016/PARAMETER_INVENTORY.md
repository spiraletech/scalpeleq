# Native parameter inventory — source inspection

All IDs, defaults, order, ranges and host normalization remain unchanged. UI
attachment callbacks continue to marshal parameter updates to the message thread.
Native definitions: Source/PluginProcessor.cpp createParameterLayout; binding:
RootsEditor::bindNode/bindCarve. All listed values retain existing automation and
state restoration. No new parameter is introduced for graph power or transitions.

| Control | Native ID/pattern | Native range, step, default | UI normalization / precision |
|---|---|---|---|
| Band gain | rootsRootGainId(root) | -60..60 dB, .01, 0 | Native linear, 0 at midpoint, 2 decimals |
| Comp threshold | rootsRootControlId(root,"comp_offset") | -9..9 dB, .1, 0 | Linear calibration offset; actual threshold derived by existing dynamics engine; 1 decimal |
| Compression | root dynamics_mode | Disabled/Off/176E/CE1B, defaultOff(1) | UI OFF/176E/CE1B maps1/2/3; legacyDisabled coerced visually toOFF by existing behavior |
| Ratio | root comp_ratio | AUTO/2/3/4/6/8/12/20:1, defaultAUTO | Existing ComboBoxAttachment |
| Carve gain | carveId(root,slot,"gain_db") | -40..40 dB, .01, 0 | Linear; legacydepth0..36 recalls as negative cut until edited; 2 decimals |
| Frequency | carve hz | 20..20000 Hz, .01, defaultCarveHz | Host skew center1000 unchanged; UI logarithmic within selected band's bounds; 2 decimals |
| Q WIDTH | carve q | .3..96 rawQ, .001, default.3 | Host skew center4 unchanged; existing UI log .3..96 presents0..100% tightness; default0.0, 1 decimal |
| Visual Decay | aura_memory | 0..10 s, .01, default.18 | Native skew center.4, 2 decimals; minimum0.00 |
| Heat map | aura_color | bool, defaulttrue | Native ButtonAttachment |
| Freeze | graph-local frozen_ | bool, defaultfalse | Existing UI-only freeze; no invented host parameter |
| MASTER | master_output_domain | Stereo/Mid/Side, defaultStereo | Existing native complete gesture command |
| XRAY | scalpel_xray | bool | Existing native ButtonAttachment |
| ON/OFF | root on / carve on | bool | Existing explicit immediate parameter gestures; no duplicate latch |
| MONO/SOLO/PHASE/LIMIT | root mono/solo/phase/limit_on | existing bools | Existing button attachments; PHASE disabled for unsupported domain |
| Reduced motion | sonar_reduced_motion | bool defaulttrue | Existing attached parameter; now also suppresses CRT/MONOLITH |

Q default0.0 is a display percentage, not zero rawQ. Comp threshold is a signed
calibration parameter, not the old unused -60..0 threshold ID. Changing either
native definition would have altered audio/automation semantics and was avoided.

Exact ID expansion uses 1-based root and slot numbers: BAND GAIN =
`roots_root1_gain_db` through `roots_root7_gain_db`; COMP THRESHOLD =
`roots_root{1..7}_comp_offset`; compression = `roots_root{1..7}_dynamics_mode`;
ratio = `roots_root{1..7}_comp_ratio`; CARVE example =
`surgery_carve_r1_c1_hz`, `_gain_db`, `_q`, `_on` through root7/slot4.
Root ON defaults true; CARVE ON defaults false; MONO/SOLO/PHASE/LIMIT default
false. Graph power, selected touchscreen mode and FREEZE remain editor-local.
No promise of their persistence through editor replacement is introduced.

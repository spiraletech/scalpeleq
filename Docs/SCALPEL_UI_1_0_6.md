# SCALPEL EQ UI 1.0.6 — pristine polish contract

Version 1.0.6 is a rendering and host-window polish pass over the approved
1.0.5 console. It does not change DSP algorithms, parameter IDs, automation
ranges, preset state, band order, or page hierarchy.

## Visual contract

- The chassis uses a warmer Night Shift-style ivory, champagne, copper, and
  espresso palette. It avoids the pink cast of 1.0.5 while preserving the
  orange SCALPEL accent.
- Every editor pixel is painted with the warm chassis edge colour before the
  rounded rack is drawn. The plug-in never contributes a black letterbox or
  exposed black corner.
- UI copy uses an explicit Windows-safe Segoe UI family request. Tabs retain
  the approved bold italic treatment; utility labels and values use restrained
  regular/bold weights.
- Knobs are vector-native: one machined bezel, one dial face, one indicator,
  and a thin value arc. The value arc begins at the parameter's double-click
  reset value, so bipolar controls show no false half-scale arc at neutral.
- The bottom-right JUCE resize grip remains disabled.

## Resize contract

| Property | Value |
|---|---:|
| Authored aspect ratio | 16:9 |
| Minimum editor size | 896 × 504 |
| Default editor size | 1344 × 756 |
| Maximum editor size | 1792 × 1008 |

User resize handles are constrained to 16:9 and clamped to the stated bounds.
This keeps the console snap-locked without disabling host-supported resizing.
A DAW's own title bar and window shadow remain host-owned.

## Release gates

1. All CTest targets pass.
2. UI regression confirms the fixed aspect ratio and exact bounds.
3. UI regression confirms all four editor corners are fully opaque and warm,
   not black.
4. pluginval strictness level 8 reports SUCCESS.
5. Windows VST3 staging and artifact upload complete.

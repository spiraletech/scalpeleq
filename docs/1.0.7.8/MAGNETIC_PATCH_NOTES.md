# Scalpel EQ 1.0.7.8 — magnetic interaction patch notes

Date: 2026-11-17 (America/Los_Angeles)

## Authority and preserved baseline

- Protected visual baseline: `ui-fidelity/band-anchored-carve-1.0.7.7`
- Inspected baseline commit: `ad546dcc1c829a83d894d7f8fe94b55c5c131d79`
- Isolated patch branch: `fix/scalpel-1.0.7.8-magnetic-final`
- Plugin identity, VST3 identifiers and all historical parameter identities
  remain unchanged.
- Chassis, panel proportions, materials, screws, typography, established
  control families and BAND / CARVE / COLOR / ANALYZE layout are unchanged.

## Implemented interaction contract

- New and migrated 1.0.7.7 sessions contain all seven visible band regions.
- Each present band has an independently draggable left and right magnet.
- The complete translucent colored interior is a drag target for rigid
  logarithmic movement; no center dot is required.
- Same-domain regions cannot invert, cross or overlap. Stereo/Mid/Side
  cross-domain overlap remains legal.
- Anchors acquire logarithmic-grid and neighboring-boundary snap targets with
  release hysteresis. Ctrl bypasses snapping for precise adjustment.
- Opposing anchors snapped at the same coordinate become a serialized shared
  boundary. Moving that boundary resizes both adjacent regions.
- Interior dragging a connected member moves the whole connected component,
  preserving member widths and shared coordinates. The group stops as one
  unit at 20 Hz, 20 kHz or an unconnected same-domain region.
- Right-click exposes explicit left/right unlinking and per-band removal.
- A removed slot hides its graph region and magnets while its BAND tab remains
  available. Double-click legal empty space to recreate that stable slot.

## Audio/state separation

`present` is an appended lifecycle parameter and is never substituted for the
historical BAND `on` parameter. A present BAND with `on = false` performs the
validated high-rejection spectral removal. An absent band fades all of its
processing back to dry and cannot become the active SOLO target.

The GUI relationship model validates edits before publishing frequencies,
presence, domain and link identifiers through the parameter state. The audio
thread consumes atomic values only; it does not traverse the relationship
graph, access GUI objects, allocate memory or take a UI lock.

Legacy states without lifecycle/link IDs migrate to seven present, unlinked
regions. New IDs are appended after all established IDs.

## Verification status

Local standalone C++20 gates currently pass for:

- seven-band factory state and delete/recreate;
- non-inverting independent anchors and same-domain collision rejection;
- cross-domain overlap;
- shared-boundary resize and explicit unlink;
- connected-group rigid movement and blocker clamping;
- deterministic model snapshot/restore;
- absent-band dry bypass remaining distinct from BAND OFF.

The exact Windows x64 VST3 build, complete CTest suite and pluginval run must
pass on this branch before its artifact can be called a candidate. FL Studio
25 project recall, automation, editor close and plugin removal remain a
separate physical-host validation gate. This document does not promote a
stable release.

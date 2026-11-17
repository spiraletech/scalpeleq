# Scalpel EQ 1.0.7.8 Technical & User Manual — candidate draft

**Status:** controlled development candidate. Stable promotion is not authorized.

## 1. Introduction and processing philosophy

Scalpel EQ processes user-defined frequency territories. Each of seven stable
band identities owns two frequency boundaries, a gain and processing profile,
and four subordinate CARVE slots. Filters have transition skirts, phase shift
and finite settling behavior; a territory is not instrument separation or a
mathematically rectangular frequency selection.

Version 1.0.7.8 preserves the approved 1.0.7.7 champagne-ivory/graphite chassis,
panel proportions, BAND / CARVE / COLOR / ANALYZE menus, hardware families and
plugin identity. Its visual changes are restricted to graph regions, magnets
and interaction feedback required by the magnetic editor.

## 2. Installation and FL Studio integration

Close FL Studio and retain the previous working VST3 bundle. Extract the ZIP
and copy the complete `Scalpel EQ.vst3` folder to:

`C:\Program Files\Common Files\VST3\`

Keep the binary at `Contents\x86_64-win\Scalpel EQ.vst3`, rescan in FL Studio's
plugin manager, and first open it on a test mixer track. The manufacturer code,
plugin code and bundle identifier remain `SpEt`, `ScE1`, and
`com.spiraletech.scalpeleq`.

Automated VST3 validation is not a substitute for FL Studio 25 project recall,
automation, editor closure and plugin-removal testing.

## 3. Band creation, removal and selection

New sessions show all seven established BAND 1–7 regions. The seven BAND tabs
always remain visible. Click a tab, magnet or colored region to select a band.

Right-click a present region and choose **REMOVE BAND** to hide that region and
its magnets. This returns the stable slot without changing unrelated bands.
Double-click legal empty graph space to recreate a removed slot. The selected
absent tab is preferred; otherwise the first available slot is used.

Removal and BAND OFF are different states. Removal bypasses the band's
processing. BAND OFF keeps the band present and applies spectral removal.

## 4. Dual magnets and magnetic grid operation

Every present band has a left and right frequency magnet. Drag either magnet to
resize the region. A magnet cannot cross its partner or enter an unrelated
same-domain region. Unlinked magnets at the same coordinate are displayed
separately around the existing rail so both remain visible and selectable.
Genuinely linked magnets merge at their shared coordinate.

Magnets snap to meaningful logarithmic-grid values and neighboring boundaries.
Acquire/release hysteresis prevents rapid target jitter. Hold Ctrl while
dragging for precise unsnapped placement.

Drag anywhere inside the translucent colored region to move the complete band
while preserving its logarithmic width. No small center dot is required.

## 5. Connected bands and group movement

Snapping opposing same-domain magnets together creates a reciprocal serialized
connection. Moving the shared magnet resizes both adjacent bands at the exact
same coordinate. Dragging the colored interior of any connected member moves
the entire connected group, preserving every member width and shared boundary.

A group stops as one unit at 20 Hz, 20 kHz, or an unconnected same-domain
region. It does not compress, overlap, or disconnect because the pointer moved
farther. Right-click and choose **UNLINK LEFT MAGNET** or **UNLINK RIGHT
MAGNET** for explicit disconnection.

Stereo, Mid and Side use separate collision spaces. Deliberate cross-domain
frequency overlap remains legal.

## 6. Spectral removal behavior and limitations

BAND OFF uses a fixed-storage 40-pole Butterworth band-stop in the band's
assigned channel domain. Two frozen filter banks crossfade over 20 ms when a
boundary changes, keeping recursive state paired with its coefficients and
avoiding audio-thread allocation.

The measured central 50% of logarithmic stopband width meets at least 60 dB
rejection in the automated 44.1, 48, 96 and 192 kHz gates. Exact mathematical
edges are approximately −3.01 dB and are not claimed as 60 dB points. Causal
IIR phase/group delay and finite ringing remain despite reported zero latency.
Retargeting briefly contains old and new responses during the crossfade.

## 7. Stereo/Mid/Side and MONO

The engine uses `M=(L+R)/2`, `S=(L−R)/2`, then reconstructs `L=M+S`, `R=M−S`.
MID removal preserves Side; SIDE removal preserves Mid when other processing is
neutral. MONO suppresses Side in the selected frequency territory rather than
collapsing the complete output. Filter skirts remain audible at boundaries.

## 8. SOLO and signal audition

One present band is authoritative for SOLO. Selection changes crossfade over
approximately 8 ms. The assigned domain controls audition: Mid excludes Side,
and Side excludes Mid. SOLO is a filtered frequency audition, not source
extraction. Deleting a band clears its SOLO state and it cannot remain the
active SOLO target.

## 9. CARVE and C1–C4

Each band owns four CARVE slots with frequency, depth, Q, threshold, dynamic
mode and channel assignment. CARVE positions are clamped back into a region
after boundary or group movement. Disabled CARVE nodes remain invisible and do
not process audio.

## 10. COLOR, compression, PHASE and LIMIT

COLOR provides Dynamic, Sparkle, Shine, Smoothening and Vintage Tape controls.
Nonlinear COLOR cannot be represented completely by a static response curve.

The FET-inspired profile uses fast detector timing and a 4:1 curve; COMP
THRESHOLD is consumed by the gain-reduction calculation. Other optical/tube
profiles use different time constants. These are inspired processors, not
certified hardware circuit models.

PHASE estimates the L/R relationship near the band's geometric center and
rotates the R estimate toward L when confidence is sufficient. It is not
universal time alignment or recovery of cancelled information.

LIMIT is program-dependent optical leveling with a soft reduction curve. It is
not a brickwall limiter or true-peak limiter.

## 11. Analyzer, X-RAY and metering

The spectrum and peak history are measured displays. Selected CARVE response
curves are theoretical linear responses and must not be read as compression,
dynamic CARVE, PHASE or nonlinear COLOR behavior. X-RAY remains visual-only;
audio-null regression tests cover its enabled/disabled path. L/R meters and the
PEAK field are sample-peak displays, not true-peak meters.

The full cumulative theoretical response for every band is not yet drawn as a
separate total curve in this candidate. Measured processed spectrum remains the
authoritative cumulative display.

## 12. Automation and preset recall

Historical parameter identities and ordering remain intact. Presence and two
magnetic-link parameters per band are appended. Frequencies, domains, presence
and reciprocal links reconstruct the graph from saved state. A 1.0.7.7 state
without the new fields migrates to seven present, unlinked regions.

Generic pluginval state restoration and parameter automation pass. Actual FL
Studio automation editing and host undo/redo remain host-validation items.

## 13. Safety, troubleshooting and known limitations

Keep the previous approved VST3 available until host testing is complete. If a
sound remains audible after removal, remember that a source may span frequencies
outside the region and that the exact boundaries are transition points.

Known/unverified items:

- FL Studio 25 playback, project recall, automation, close and removal;
- long-session behavior with rapid magnetic edits;
- host undo grouping for multi-parameter connected moves;
- broadband/transient PHASE behavior across every domain/effect combination;
- a separate full cumulative theoretical response curve.

## 14. Engineering verification and technical specifications

| Item | Candidate evidence |
| --- | --- |
| Protected baseline | `ad546dcc1c829a83d894d7f8fe94b55c5c131d79` |
| Framework | JUCE 8 / C++20 / Windows x64 VST3 |
| Reported latency | 0 samples; causal IIR group delay remains |
| Sample rates gated | 44.1, 48, 96 and 192 kHz |
| Stopband release gate | 174 probes; zero interior 60 dB failures |
| Out-of-band gate | zero 0.1 dB failures |
| Signal/safety gate | impulse, noise, multitone, sweep and live movement pass |
| Audio-thread allocation gate | zero allocations in tested removal path |
| Magnetic model | factory seven, delete/recreate, dual anchors, group move, shared resize, unlink, cross-domain overlap and restore pass |
| Windows CTest | 36/36 pass on code candidate |
| pluginval | SUCCESS, strictness 8, seed 12345 |
| FL Studio 25 | Not tested in this environment |

Passing automated gates establishes a review candidate only. Merge, stable
release and canonical promotion require separate approval from Agnathos.

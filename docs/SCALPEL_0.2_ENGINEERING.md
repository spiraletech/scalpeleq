# Scalpel EQ 0.2.0 engineering notes

Based on the user's current commit `eaa47f0971ae8044592f9b2b5146c8faf69b1d6d`
on `feature/xray-3d-fluid`. Surgery EQ and the earlier Scalpel main are preserved.

## Rendering and assets

Preserve the wide ivory layout, approved dial/flame/snowflake art and absence of
external JUKE rails. Embed the supplied 1254x1254 screw master. Resize through
3840x2160; vector chassis, labels and controls follow host DPI. GPU output and CPU
material fallback allocate at the graph's physical pixel size, capped at 3840x2160.
The flow simulation remains 64x40x32; higher output resolution does not increase
simulation-grid detail or certify photographic parity with the references.

Offscreen compute now calls `Flush()` after readback copies. There is no swap-chain
Present; a nonblocking staging Map alone left supported hardware at GPU WARMING.
Submission does not wait for the GPU and never runs on the audio thread.

A continuous reflective surface sits in front of the transported volume: measured
energy controls height, folded normals, color film and sparse reflective beads.
This is an artistic material model, not a validated multiphase solver. Grid, axes,
selection and warnings are composited after the material. Freeze holds geometry
and simulation, and repeated paints do not multiply elapsed simulation time.

Native full-editor software snapshots exceed a 16.7 ms frame; see the recorded CSV
timings. 60 FPS remains an optimization target, not a certification. The supplied
reference screenshots are retained at their supplied resolution, not called 4K art.

## Controls and DSP

- COMP AMOUNT reverses slider presentation: clockwise lowers the existing stored
  threshold from 0 toward -60 dBFS. Amount is shown positively. IDs, compressor-off
  behavior and physical preset values remain intact.
- Existing FET profile is named `1176 E 4:1`: ratio 4:1, attack 0.8 ms, release
  140 ms, knee 2 dB. This is Rev E-inspired, not measured component-level emulation.
  [Manufacturer timing context](https://www.uaudio.com/blogs/ua/1176-collection-tips).
- FIELD labels STEREO/MID/SIDE. DYNAMIC remains per-band detector routing; COLOR
  DYNAMIC remains an independent processing amount.
- Host name is PHASE AUTO CORRECTION. Its conservative algorithm reduces negatively
  correlated band SIDE energy; it cannot infer unknown delays or reconstruct
  missing information lost through cancellation.
- LIMIT retains the program-dependent optical leveler inspired by the LA-2A.
  It is not a brickwall or true-peak ceiling.
- CARVE is bipolar -90..+90 dB: positive depth still cuts, negative depth boosts.
  DSP enable gates, coefficient bounds, response plot, marker and drag geometry
  support both signs. Zero-depth markers share the zero-response rail.
- Physical preset depths retain meaning. **Expanded normalized depth automation
  scaling differs from 0.1.x**; review old host automation lanes. Extreme boosts
  can exceed full scale; LIMIT is not a ceiling.
- Retain per-band measured peak warnings and gentle red pulses from the supplied
  current commit, including its measured-frame gating.

## Synesthesia accuracy

Frequency, pitch-class salience, level and musical features drive consistent visual
mappings. The palette is a design choice. Coarse FFT pitch-class estimates are not
polyphonic melody transcription and do not measure a user's synesthetic experience.
Solfeggio and audio/optical wavelength folding are not presented as neuroscience.

[Ward, Huckstep & Tsakanikos (2006)](https://pubmed.ncbi.nlm.nih.gov/16683501/)
experimentally examined internally consistent sound-color associations and shared
pitch/lightness matching tendencies. This supports an inspired visual mapping;
it does not establish a universal note-to-hue table.

## Removal safety and verification

Real VST3 validation reproduced a post-SUCCESS DLL-unload deadlock on the local
NVIDIA driver. A captured native stack showed driver waits under loader shutdown.
The static PNG ImageCache retained native Direct2D images until CRT/DllMain cleanup.
Load all static artwork into SoftwareImageType instead, without leaving a native
ImageCache entry behind. Editor-owned GPU simulation and output remain accelerated.
The native test checks all four artwork types. `scripts/Validate-HostUnload.ps1`
runs separate GUI pluginval processes, requires both a SUCCESS report and exit 0,
and fails on a timeout; SUCCESS printed before a shutdown hang is not sufficient.

Stop the editor timer, cancel its chooser, disable graph selection callbacks and
release visual resources explicitly before member destruction. Chooser callbacks
check both a closing flag and JUCE SafePointer. Clear D3D context bindings and submit
remaining commands before releasing COM resources. No deferred plugin callback or
background GPU worker is introduced; staging readback stays nonblocking.

Retain the processor's clean FFT-worker join on release, prepare and destruction;
never force-kill it. Cancel pending parameter AsyncUpdater work and remove listeners
before processor members disappear.

Dedicated removal regression exercises X-RAY active, running audio, pending timer
and parameter callbacks, editor destruction, callback drain, processor destruction,
restart and repeated release/prepare. The inherited suite also repeats it.
Native UI validation renders all four panels at 1440x810 and 3840x2160, checks live
GPU dimensions where available, visible input, exact frozen pixels, clockwise amount,
FIELD, PHASE name and bipolar recall. Assert-based Scalpel DSP tests stay enabled
in Release. Extreme response/DSP endpoints and marker round trips are tested.

Automated native tests and pluginval reduce risk; they cannot prove removal safety
in every DAW/driver combination. Physical FL Studio mix-rack removal, transport
stress, device DPI changes and project reopen remain host acceptance checks.

# X-RAY Symbiote audit and implementation plan

Baseline: `feature/xray-3d-fluid` at `eaa47f0971ae8044592f9b2b5146c8faf69b1d6d`.
This branch begins with the measured under-curve mask and readback diagnostics.
It is an experimental review branch, not a verified Windows release.

## System state and root causes

| Question | Code evidence and conclusion |
| --- | --- |
| Tar appearance | `RootsEditor.cpp` paints several opaque gray stops over the entire graph. `RenderVolume` in `ScalpelFluid3D.cpp` ray marches 32 slices with high extinction, repeated high spatial frequency sine interference, and a bright near-white normal/specular lobe. `AdvectPhases` injects material near the floor using `exp(-y*.061)` and makes bubble density from flux. These mechanisms explain the heavy black/silver folds; visual confirmation still requires host screenshots. |
| Weak internal color | `phasesOut.z` carries a **single pigment amount**, not RGB pigment. `RenderVolume` reconstructs color from the instantaneous `spectral[x].yzw`, multiplies by energy `drive`, then composites behind opaque dark material and white specular. The current code therefore does not transport hue through 3D space. Chroma only biases source colors when the editor builds the 64 columns. |
| GPU frames displayed? | Initialization compiles six `cs_5_0` shaders and creates a hardware D3D11 device. Dispatch renders into a UAV, copies through a three-slot staging ring, and nonblocking `Map` fills a JUCE image. `hasFrame()` then selects it for paint. `GPU WARMING` only means resources initialized; `GPU READBACK READY` now means at least one copy completed. No code proves that FL Studio composited or presented that frame to the screen. No GTX 1650 run is available here. |
| Heatmap | The baseline mapped measured log-frequency columns into a 22–48 px bottom strip with a tinted backing, unrelated to the FFT's vertical contour. The branch uses a transparent full-graph image and a per-column mask beneath the displayed contour; brightness is derived from unsloped measured dBFS. Silence and the -60 dBFS floor produce zero alpha. |
| Retain | Log-frequency mapping, analyzer handoff, 0 to -60 dBFS graph, ivory FFT and handles, confidence-gated chroma estimate, D3D11 hardware init, fixed 30 Hz simulation step, three-slot nonblocking readback, and CPU fallback. |
| Replace | Black/silver material and high-frequency folds, scalar-only pigment, bubble source, unverified fluid physics claims, and GPU-frame visibility assumption. The baseline 24-step pressure projection has no demonstrated numerical stability or conservation test. |

The simulation is **physically inspired**. Semi-Lagrangian advection and a Jacobi pressure projection are present, but volume fractions are source-injected and clamped, so numerical conservation has not been established.

## Layer contract

Estimates are budgets to validate on the GTX 1650, not measured timings. All visual work is on the editor/render path; DSP parameters and audio processing remain independent.

| Layer | Input → output | Owner | Target budget at 640 × 224 |
| --- | --- | --- | --- |
| L0 coordinates | 20 Hz–20 kHz, 0 to -60 dBFS → stable graph positions | CPU editor | <0.1 ms |
| L1 aurora | Measured dBFS + visible contour → transparent under-curve energy mask | CPU initially; GPU later | <1.0 ms |
| L2 depth | Energy-gated sources → low-frequency shadow/density volume | D3D11 compute | <0.5 ms |
| L3 liquid body | Bounded velocity and density → broad depth-separated swells | D3D11 compute | <2.0 ms |
| L4 optics | Density gradient → bounded refraction offset; no coordinate shift to L0/L8 | GPU render | <0.5 ms |
| L5 pigments | Confidence-gated palette + energy → independently advected RGB or multiple pigments | GPU compute | <1.0 ms |
| L6 disturbances | Flux/transients → optional small ripples with reduced-motion gate | GPU compute | <0.3 ms |
| L7 lighting | Normals/density → soft cinematic reflection, no metallic streaks | GPU render | <1.0 ms |
| L8 measurements/UI | Analyzer samples and parameters → ivory FFT, handles, labels | JUCE editor | <1.0 ms |

The total must include GPU simulation, copy, CPU pixel conversion, JUCE paint, and host composition. These targets do not establish 60 FPS. A 16.67 ms frame budget must be measured at the host, including missed deadlines and p95/p99 frame intervals.

## Implementation phases and acceptance

1. **Truthful mask and baseline telemetry (this PR):** full-height measured under-curve heatmap; clip decorative GPU image to the FFT; count submissions, pending copies, successful readbacks, errors, and a smoothed successful-readback rate. Keep the existing shader and fallback during this stage.
2. **Presentation trace:** log HRESULT and feature level for device/compile/resource failures, timestamp dispatch/readback/paint, distinguish new image from stale cached image, and measure host-presented frames with a Windows/FL capture or graphics debugger. Never infer actual presentation from a label.
3. **Liquid transport:** replace `phase.z` with actual independently advected pigment channels; use bounded low-frequency velocity sources and three depth bands. Test finite values, silence decay, stability, approximate volume behavior, and reduced motion before invoking any stronger physics claim.
4. **Optical redesign:** separate shadow, refraction, color absorption, and broad specular passes. Tune against reference screenshots for translucent obsidian, aurora currents, and restrained highlights. Remove bubble accumulation and high-frequency sine wrinkles. Keep RGB hue aesthetic, never a statement about physical audio colors.
5. **Host validation:** compile every HLSL entry on Windows CI, run analyzer/mask/DSP regressions, capture the actual VST in FL Studio on GTX 1650, log measured delivered FPS and p95/p99 timings, compare CPU fallback, and repeatedly insert/remove the plugin while watching for leaks or unload crashes. Only then consider release readiness.

## Verification and unresolved items

- Local: standalone C++ under-curve regression and `git diff --check`. Neither compiles the Windows shader nor starts the plugin.
- Windows CI currently compiles embedded HLSL through `FluidVolume3D::validateShaderSource()` in `QualityTests.cpp`; it sets `SCALPEL_GPU_DISABLE=1`, so it cannot validate hardware execution.
- No actual FL Studio screenshot, GTX 1650 frame rate, GPU presentation trace, or repeated insertion/removal result is available from this Linux workspace.
- No parameter IDs or audio DSP files are changed in this branch.

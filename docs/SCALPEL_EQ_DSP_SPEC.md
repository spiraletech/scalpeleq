> Historical 0.1.0 first-pass contract. Current behavior and verification scope: [0.2 engineering notes](SCALPEL_0.2_ENGINEERING.md).

# Scalpel EQ — engineering contract (0.1.0 development)

Source: Surgery EQ 1.0.12 Standard+ commit `f3a816060520b0c0e87be466ecdb4d2d34f654a8`.
This document specifies the **implemented** first DSP pass, not claims of a
finished hardware emulation or clinically beneficial frequencies.

## Five per-band COLOR controls

For each of 7 ROOT/BAND territories, the values are normalized by
`u = clamp(percent/100, 0, 1)` and tapered using
`q(u) = u * (0.45 + 0.55*u)`. 0% produces a true neutral output and
the five processor gains are independent.

| Control | Real processing equation/constraint |
| --- | --- |
| **DYNAMIC** | Independent 3 ms and 75 ms power envelopes, `f += af*(x²-f)` and `s += as*(x²-s)`; `c=clamp((sqrt(f)-sqrt(s))/(sqrt(s)+0.015),-1,1)`; `GRdB=q*(c>=0?3*c:2*c)`. Processed `y=x*10^(GRdB/20)`, bounded **+3 to −2 dB**. No modulation of the other four knobs. |
| **SPARKLE** | Band-selective high-frequency drive, two low-pass stages and first-order antiderivative anti-aliasing of a tanh residual. Add `q*0.75*nyquistGuard*(0.35+0.65*transient)*(residual-DC)`. Bounded and **not alias-free**. |
| **SHINE** | Linear air/presence shelf via `y += q*0.33*(x-LP1600(x))`. The high-frequency asymptote is **+2.48 dB**. No nonlinear saturation. |
| **SMOOTHENING** | Level- and edge-dependent HF subtraction `y -= q*0.28*activity*(0.65+0.35*transient)*(x-LP4200(x))`, high-frequency reduction bounded to approximately **−2.85 dB** in the maximum activity limit. |
| **VINTAGE TAPE** | Restrained nonlinear tape residual using ADAA tanh drive 1.7, two input poles, DC removal, plus gentle HF rounding. `y += q*(0.60*(ADAAresidual-DC)-0.045*(x-air))`. Preserve unity small-signal dry path. |

All processing is per-BAND, on the assigned MID/SIDE domains, not master-bus
COLOR. Coefficient and amount smoothing is ~10 ms. Default amounts are 0.0%.
Existing `onyx_sparkle`, `onyx_shine`, `onyx_smooth`, `onyx_tape` parameter
IDs remain stable for historical preset recall; only their displayed identity
becomes COLOR. The additional `color_dynamic` ID appends independently.

## Dynamics, PHASE, LIMIT

- **1176 Rev E-inspired compressor**: inherited FAST 4:1 FET profile,
  approximately 0.8 ms attack / 140 ms release, 4:1 ratio, 2 dB knee.
  This is inspired by the device class, not measured component-level Rev E
  emulation; more hardware-specific testing remains.
- **PHASE**: stereo coherence guard on each frequency territory. Let the
  smoothed powers of MID and SIDE be `Pm`/`Ps`,
  `corrApprox=(Pm-Ps)/(Pm+Ps+1e-12)`; when negative, subtract
  `clamp(-0.85*corrApprox,0,0.85)*SIDE`, with 10 ms parameter smoothing.
  No polarity inversion. This mitigates anti-correlated energy but cannot
  reconstruct unknown phase, delays or interference cancellation.
- **LIMIT**: optional per-BAND optical leveler with one amount dial;
  peak envelope attack ~10 ms, program-sensitive release 350–1000 ms;
  normalized amount `a` maps threshold to `−6−24a dBFS` and applies
  `reductionDb = 0.70*a*max(0,inputLevelDb-threshold)`.
  Smooth amount at 10 ms. This is **not** a brickwall/true-peak limiter.
- **Peak warning**: per-territory processed signal block sample peak is
  compared against −0.01 dBFS; the corresponding BAND tab pulses red with
  a 750 ms visibility hold, like an analog record indicator. This is an
  instrument warning, not prevention of clipping.

## X-RAY contract

- Graph presentation only. Reuse exactly the existing FFT spectral bars and
  JUKE weather/synesthesia meaning; no separate FFT, no audio-DSP branch.
- Locked graph labels `0 −5 −10 −15 −20 −25 −30 −35 −40 −50 −60` dBFS,
  independent of the −144 dBFS measurement/precision state.
- Deterministic black-mercury fill clipped under the actual spectrum curve;
  iridescent outline/bubbles under the fill, their motion driven by
  `E=0.30*level + 0.35*bass + 0.20*flux + 0.15*stereoWidth`.
- 31 bounded reflective bubbles per graph frame (initial implementation),
  hold on Freeze, Reduced Motion limiting, and silent-state settling.
- 60 Hz redraw target, not yet a certified whole-editor 60 FPS throughput.

## Verification completed and remaining

- Local GNU C++20: five COLOR controls change audio independently; zero
  neutral, deterministic legacy 4-knob compatibility and finite audio at
  44.1/48/96/192 kHz.
- Local GNU C++20: PHASE guard reduces anti-correlated SIDE energy and
  optical amount reduces loud-band level, both with finite outputs.
- Inherited ROOTS standalone suite: neutral bypass bit exact, root gain,
  root audition crossfade, compressor, mono and COLOR isolation.
- Pending: Windows JUCE VST3 compiler/CI results, pluginval, physical host
  verification, DAW automation mapping, serial preset migration, nonlinear
  spectral THD/IMD/alias measurement, GUI screenshots, full 60 Hz profiling.

# Scalpel EQ 1.0.7.8 — local DSP evidence

Candidate branch: `fix/scalpel-1.0.7.8-dsp-only`

## Portable regression result

The Release C++20 suite was configured with:

```text
cmake -S . -B build-portable -G Ninja \
  -DSCALPEL_PORTABLE_TESTS_ONLY=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-portable --parallel 2
```

All 30 registered tests passed in the final source state. The long integrated
spectral gate and the remaining gates were also run independently so failures
could not be hidden by a combined summary:

- 27 inherited/domain/transition/performance tests: 27/27 pass;
- integrated spectral release gate: pass in 96.94 s;
- experimental impulse/noise/multitone/sweep/allocation gate: pass;
- measured steady-sine gate: pass.

Assertions were explicitly re-enabled in Release builds.

## Measured results

| Gate | Final result |
| --- | ---: |
| Worst integrated interior rejection | -66.1294 dB, Mid and Side |
| Worst-case location | 44.1 kHz; 10–20 kHz band; 11,892.1 Hz probe |
| Worst integrated out-of-band change | 0.000513621 dB absolute |
| Unselected Mid/Side-domain change | <= 0.000000027 dB absolute |
| Live-drag peak, -12.04 dBFS input | 0.290994 maximum |
| Live-drag maximum adjacent-sample change | 0.0141882 |
| Audio-thread allocations in signal tests | 0 |
| All-seven BAND OFF real-time ratio | 0.12559 on this Linux reference host |

The real-time ratio processed one second at each of 44.1, 48, 96 and 192 kHz
with all seven removal bands engaged: 4.0 seconds of audio in 0.502361 seconds.
It is a host-specific engineering measurement, not a universal CPU guarantee.

Impulse, white noise, multitone, sweep and live-boundary movement remained
finite at all four sample rates. Exact mathematical band edges remain about
-3.0103 dB by filter definition; the >=60 dB claim applies only to the
declared central 50% of logarithmic interval width.

## Visual and state drift check

The following approved 1.0.7.7 visual sources compare byte-for-byte clean
against baseline `ad546dcc1c829a83d894d7f8fe94b55c5c131d79`:

- `Source/RootsEditor.cpp` and `.h`;
- `Source/PluginEditor.cpp` and `.h`;
- `Source/ScalpelUI105.h`;
- `Source/ScalpelReferenceLayout.h`.

No parameter definition or serialization source was changed.

## Pending gates

The complete JUCE target could not be configured in the Linux workspace
because its image lacks FreeType/X11/ALSA development headers. That is an
environment dependency failure, not a source compile result. The authorized
Windows x64 workflow must still compile the VST3, run the complete CTest suite
and pass pluginval strictness 8 before a ZIP is produced.

FL Studio 25 playback, automation, preset/project recall, editor close,
plugin removal and long-session behavior remain unverified until tested in the
actual host. No stable-release or canonical-promotion claim is made.

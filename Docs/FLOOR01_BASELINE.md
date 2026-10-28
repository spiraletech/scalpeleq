# Floor 01 — reproducible analyzer baseline

Source baseline: `feature/xray-symbiote-liquid-canvas` at `eb33aa4`.
This Floor 01 sequence adds analyzer telemetry and a visual-only GPU stability
hotfix. It does not alter audio DSP, plugin parameter IDs, or EQ controls.

## Stability hotfix for the next test build

The subsequent Floor 01 hotfix disables invocation of the experimental D3D11
volumetric renderer from X-RAY painting and removes its GPU readiness label.
The CPU presentation remains active. The D3D11 implementation stays in the
source tree, but is not invoked by this build. This mitigates a suspected GPU
shutdown issue; it does not prove the FL Studio unload crash is resolved.
The unload regression now alternates X-RAY ON/OFF and creates snapshots while
destroying/recreating editors and analyzer workers.

## Build and regression gates

On Windows (Visual Studio 2022, x64):

```powershell
cmake -S . -B build -A x64 -DPRISMVST_BUILD_TESTS=ON
cmake --build build --config Release --parallel 2
$env:SCALPEL_GPU_DISABLE = '1'
ctest --test-dir build -C Release --output-on-failure
Remove-Item Env:SCALPEL_GPU_DISABLE
```

The Windows CI pluginval step uses `--skip-gui-tests`, so a passing result
does **not** establish FL Studio GUI or GPU correctness.

## Host evidence required before Floor 01 passes

- Insert and remove Scalpel EQ repeatedly in FL Studio with X-RAY both off and on.
- Open, close, resize, freeze and unfreeze the editor while audio plays.
- Record OS, FL Studio version, GPU/driver, sample rate and block size.
- Record `getAnalyzerHealth()` before and after test playback. Any dropped
  samples indicate that analyzer visualization may be missing audio, even if
  the plugin's audible output is unaffected.
- Measure *presented* frames and p95/p99 frame times; do not substitute timer
  cadence or completed GPU readback rate for host presentation.
- Check for audio dropouts, device loss, unload crashes and leaked resources.

## Baseline caveats

Current FFT size is 4096 with a 384-sample hop. The display receives 256
lab bands. Independent calibration tests and real GTX 1650/FL Studio profiling
remain prerequisites to treating Floor 01 as fully verified.

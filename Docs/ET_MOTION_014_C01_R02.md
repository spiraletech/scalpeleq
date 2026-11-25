# ET-MOTION-014 — C01 R02 native integration

Development review only. Product version remains **1.0.8.7**. No release or remote push is authorized by this implementation.

## Pinned baseline

- Repository: `spiraletech/scalpeleq`.
- Verified remote base branch: `ui-fidelity/scalpel-1.0.8.7-oled-readouts`.
- Base commit: `3639c95b063d5ad7a1941574c6116dc87cb4e07c`.
- Local review branch: `ui-fidelity/aura-c01-r02-et-motion-014`, initially at that same commit; changes remain uncommitted.
- Original dirty workspace: `50f400ef7055cd9877cb7c5181df77349d06ea2b`. It was not reset, switched or overwritten. Its uncommitted ANALYZE source/assets are byte-identical to the corresponding content already in the verified remote base. A complete original workspace backup and per-file hashes were recorded before integration.

## Renderer and registration

The 47 PNG files in `Assets/Scalpel/AuraFoundry/C01_R02` are byte-preserving copies from the supplied R02 package: the 46 registered 4× layers and the 4× five-state atlas. Only filename namespaces changed. `ASSET_PROVENANCE.json` maps them to their supplied paths and SHA-256 digests. No SVGs, replacement fonts or generated material are used by the native renderer.

`ScalpelAuraPlush.h` prepares immutable software images in the BAND button constructor on the UI thread. It caches 572 × 200 pixel atlas cells (143 × 50 reference units at 4×). No 408 × 124 legacy BAND addressing is used. The legacy page-tab renderer and all lower controls retain their existing artwork and layout.

All seven native slots remain 143 × 45 logical pixels. Visible material is fitted uniformly with `s=min(width/135,height/42)`, currently `143/135`. The material occupies 143 × 44.488889, centered vertically at 0.255556. The padded texture origin is `(-4s, 0.255556-4s)`. Production gaps remain 5,5,4,5,5,5 pixels. Editor dimensions remain 1344 × 756; the existing host/editor transform is retained.

During motion, registered source images are at **zero travel**. Housing and indicator use the fixed common mapping. The surface uses `T(0,4.6+d) S(1,(40.8-d)/40.8) T(0,-4.6)`. Lettering uses `T(0,d)`. At exact travel endpoints the renderer uses the supplied composite PNG cell. There is no whole-texture translation and no extra displacement of pre-compressed endpoint art.

## Authority and interaction

`graph_.selectedNode()/2` remains the single selection authority. Existing toggle-state mirroring remains presentation-only. The existing `onClick -> graph_.selectNode(root*2)` and popup callbacks are unchanged. No new selection, DSP, gesture or completion callbacks are introduced.

Native state precedence remains disabled, pressed, selected, hover, idle. `RootSelectorButton::buttonStateChanged` retargets only a visual travel value. The existing editor update calls `advanceAura`; it adds no timer. Travel uses the monotonic high-resolution clock and 160 ms smoothstep. A changed target starts from the current sampled position. An unchanged target never restarts. The 1.75 reference pixel travel becomes 1.853704 logical pixels in the existing slots. The experimental 230 ms timing is not implemented.

The native JUCE event implementation still handles mouse release, exit, keyboard activation, enablement and capture semantics. No new handlers replace those paths. The existing keyboard-only focus affordance is retained. The new BAND paint path uses the approved R02 housing and indicator; it does not add the superseded peak-warning perimeter. Existing graph peak warnings remain unchanged.

## Build and regression entry points

The existing Windows pipeline is unchanged:

```powershell
cmake -S . -B build -A x64 -DPRISMVST_BUILD_TESTS=ON
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure
```

New tests are `scalpel_aura_motion` and `scalpel_aura_c01_native` in the full build. Portable builds add `ScalpelAuraMotion` to the existing standalone suite. The native harness is also callable as `prismvst_tests --aura-c01`. It renders the actual seven BAND components in all five states, records real elapsed-time samples, and exercises native callback and management paths. Modal result IDs in this test invoke the existing REMOVE/ADD callback; they are not simulated physical menu clicks.

The complete Linux full-editor capture/regression encounters a text-shaping crash also reproduced on the untouched baseline. Native BAND captures are isolated actual JUCE component snapshots, not full-editor screenshots or visual mockups. Consult the review package's PROBE report for measured results and limits.

Windows x64 VST3 compilation, Windows native capture, pluginval and FL Studio 25 require the existing Windows workflow or a Windows workstation. This Linux workspace does not establish those results. Running GitHub Actions with these uncommitted changes requires a separately authorized commit and push to the review branch. No existing remote artifact represents these changes.

## Rollback

The original dirty working directory was left intact. The isolated review worktree can simply be left unused without changing that baseline. For an applied review patch elsewhere, first verify the recorded base and run `git apply --reverse --check INTEGRATION.patch`. Reverse it only if the check succeeds and no later edits would be lost. The review package includes scoped baseline preimages and hashes; do not use a destructive reset or discard unrelated work.

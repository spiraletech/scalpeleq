#!/usr/bin/env python3
"""Compile all JUCE-independent baseline tests with assertions enabled."""
import pathlib, subprocess, json, sys
root=pathlib.Path(__file__).resolve().parents[1]
out=root/'evidence'/'1.0.7.8';out.mkdir(parents=True,exist_ok=True)
build=root/'build-portable';build.mkdir(exist_ok=True)
# These are exactly CMake's add_executable(... Tests/*.cpp) targets that do
# not include JuceHeader.h or require processor/editor translation units.
names=['ScalpelDbAxis','ScalpelAnalyzerHealth','ScalpelUnderCurve','ScalpelMercury',
       'ScalpelChroma','ScalpelColor','ScalpelBand','RootsCore','PrismLabAnalyzer',
       'RootsRealtime','RootsUiSpectrum','RootsParentControls','SurgeryCarve',
       'SurgerySonar','SurgeryHeaderLayout','SurgeryNodeFocus','SurgeryRootRouting',
       'SurgeryResponse','SurgeryAnalysis','SurgeryPrecision','SurgeryDualKnobDSP',
       'SurgerySpectrogram','SurgerySpectrumFill','SurgerySpectralColours']
results=[]
for name in names:
    target=build/name
    compile=subprocess.run(['g++','-std=c++20','-O2','-Wall','-Wextra','-pthread',str(root/'Tests'/(name+'Tests.cpp')),'-o',str(target)],capture_output=True,text=True)
    run=subprocess.run([str(target)],capture_output=True,text=True) if compile.returncode==0 else None
    result=dict(test=name,compile_exit=compile.returncode,run_exit=run.returncode if run else None,compiler_output=compile.stdout+compile.stderr,output=run.stdout+run.stderr if run else '')
    results.append(result);print(name, 'PASS' if run and run.returncode==0 else 'FAIL',flush=True)
(out/'portable-regressions.json').write_text(json.dumps(results,indent=2)+'\n')
sys.exit(any(r['compile_exit']!=0 or r['run_exit']!=0 for r in results))

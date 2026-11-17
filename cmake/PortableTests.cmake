# Native C++20 DSP/core tests; usable without JUCE, graphics or audio drivers.
enable_testing()
set(SCALPEL_PORTABLE_TESTS
  ScalpelDbAxis ScalpelAnalyzerHealth ScalpelUnderCurve ScalpelMercury
  ScalpelChroma ScalpelColor ScalpelBand RootsCore PrismLabAnalyzer
  RootsRealtime RootsUiSpectrum RootsParentControls SurgeryCarve
  SurgerySonar SurgeryHeaderLayout SurgeryNodeFocus SurgeryRootRouting
  SurgeryResponse SurgeryAnalysis SurgeryPrecision SurgeryDualKnobDSP
  SurgerySpectrogram SurgerySpectrumFill SurgerySpectralColours
  ScalpelRemovalDomain ScalpelBandStopTransition ScalpelBandStopPerformance
  ScalpelBandModel)
foreach(name IN LISTS SCALPEL_PORTABLE_TESTS)
  add_executable(${name} Tests/${name}Tests.cpp)
  # The inherited assertions are meaningful only when NDEBUG is absent.
  if(MSVC)
    target_compile_options(${name} PRIVATE /UNDEBUG)
  else()
    target_compile_options(${name} PRIVATE -UNDEBUG)
  endif()
  add_test(NAME ${name} COMMAND ${name})
endforeach()
add_executable(scalpel_spectral_gate Tests/ScalpelSpectralGateTests.cpp)
add_test(NAME scalpel_spectral_release_gate COMMAND scalpel_spectral_gate ${CMAKE_CURRENT_BINARY_DIR}/spectral-gate.csv)
add_executable(scalpel_bandstop_experiment Tests/ScalpelBandStopExperiment.cpp)
add_test(NAME scalpel_experimental_audio_safety_gate COMMAND scalpel_bandstop_experiment ${CMAKE_CURRENT_BINARY_DIR})
set_tests_properties(scalpel_spectral_release_gate scalpel_experimental_audio_safety_gate
  PROPERTIES LABELS "release_gate" TIMEOUT 180)

add_executable(scalpel_candidate_measured Tests/ScalpelBandStopMeasuredTests.cpp)
add_test(NAME scalpel_candidate_steady_sine_measurements COMMAND scalpel_candidate_measured ${CMAKE_CURRENT_BINARY_DIR}/candidate-measured.csv)

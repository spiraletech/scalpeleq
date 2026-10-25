cmake_minimum_required(VERSION 3.24)
project(SCALPELEQ VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(FetchContent)
FetchContent_Declare(
  JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.4
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

set(PRISMVST_VST2_SDK_PATH "" CACHE PATH "Path to a licensed Steinberg VST2 SDK include directory")
set(PRISMVST_FORMATS VST3)

if(PRISMVST_VST2_SDK_PATH)
  juce_set_vst2_sdk_path("${PRISMVST_VST2_SDK_PATH}")
  list(APPEND PRISMVST_FORMATS VST)
  message(STATUS "PRISMVST VST2 build enabled: ${PRISMVST_VST2_SDK_PATH}")
else()
  message(STATUS "PRISMVST VST2 build disabled. Set PRISMVST_VST2_SDK_PATH to build PRISMVST.dll")
endif()

juce_add_plugin(PRISMVST
  COMPANY_NAME "Spiral Ether Tech"
  BUNDLE_ID "com.spiraletech.scalpeleq"
  PLUGIN_MANUFACTURER_CODE SpEt
  PLUGIN_CODE ScE1
  FORMATS ${PRISMVST_FORMATS}
  PRODUCT_NAME "Scalpel EQ"
  VST3_CATEGORIES Fx Mastering
  COPY_PLUGIN_AFTER_BUILD FALSE
)

juce_generate_juce_header(PRISMVST)
# Pinned upstream artwork used solely by the inherited JUKE renderer. The
# standalone Scalpel UI itself is vector-drawn; binary assets will be promoted
# into Scalpel-owned art after the first reproducible CI baseline.
set(SCALPEL_ART_DIR "${CMAKE_CURRENT_BINARY_DIR}/scalpel-source-art")
file(MAKE_DIRECTORY "${SCALPEL_ART_DIR}")
file(DOWNLOAD
  "https://raw.githubusercontent.com/spiraletech/prismvst/f3a816060520b0c0e87be466ecdb4d2d34f654a8/Assets/OnyxRack/chassis-1.0.3.png"
  "${SCALPEL_ART_DIR}/chassis-1.0.3.png"
  EXPECTED_HASH SHA256=d7fafc0a7c04235cfb77698f1ac843cb5258dd091fa0bbab1450985cd1820342
  STATUS SCALPEL_CHASSIS_STATUS)
list(GET SCALPEL_CHASSIS_STATUS 0 SCALPEL_CHASSIS_ERROR)
if(NOT SCALPEL_CHASSIS_ERROR EQUAL 0)
  message(FATAL_ERROR "Pinned Surgery chassis unavailable: ${SCALPEL_CHASSIS_STATUS}")
endif()
file(DOWNLOAD
  "https://raw.githubusercontent.com/spiraletech/prismvst/f3a816060520b0c0e87be466ecdb4d2d34f654a8/Assets/StormChassis/approved-reference.png"
  "${SCALPEL_ART_DIR}/approved-reference.png"
  EXPECTED_HASH SHA256=966b3b6534c2e2f57d6702df11d3c00053de1ece7df4dac92b2f9c955e788bcb
  STATUS SCALPEL_STORM_STATUS)
list(GET SCALPEL_STORM_STATUS 0 SCALPEL_STORM_ERROR)
if(NOT SCALPEL_STORM_ERROR EQUAL 0)
  message(FATAL_ERROR "Pinned Surgery JUKE reference unavailable: ${SCALPEL_STORM_STATUS}")
endif()
juce_add_binary_data(SurgeryRackAssets SOURCES
  "${SCALPEL_ART_DIR}/chassis-1.0.3.png"
  "${SCALPEL_ART_DIR}/approved-reference.png"
  Assets/Scalpel/scalpel-knob.png
  Assets/Scalpel/scalpel-freeze.png
  Assets/Scalpel/scalpel-heatmap.png)
target_link_libraries(PRISMVST PRIVATE SurgeryRackAssets)

target_sources(PRISMVST PRIVATE
  Source/PluginProcessor.cpp
  Source/SurgeryWeatherAnalysis.cpp
  Source/PluginProcessor.h
  Source/PluginEditor.cpp
  Source/PluginEditor.h
  Source/RootsEditor.cpp
  Source/RootsEditor.h
  Source/ScalpelMercuryField.h
  Source/ScalpelFluid3D.h
  Source/ScalpelFluid3D.cpp
  Source/RootsAudioEngine.h
)

target_compile_definitions(PRISMVST PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
)

if(WIN32)
  # Compute shaders are compiled from embedded HLSL when X-RAY is activated.
  # D3D11 resources belong to the editor, never to the real-time DSP thread.
  target_link_libraries(PRISMVST PRIVATE d3d11 dxgi d3dcompiler)
endif()

target_link_libraries(PRISMVST PRIVATE
  juce::juce_audio_utils
  juce::juce_dsp
  PUBLIC
  juce::juce_recommended_config_flags
  juce::juce_recommended_lto_flags
  juce::juce_recommended_warning_flags
)

option(PRISMVST_BUILD_TESTS "Build DSP smoke tests" ON)
if(PRISMVST_BUILD_TESTS)
  add_executable(scalpel_mercury_tests Tests/ScalpelMercuryTests.cpp Source/ScalpelMercuryField.h)
  target_compile_features(scalpel_mercury_tests PRIVATE cxx_std_20)
  add_test(NAME scalpel_xray_liquid_surface COMMAND scalpel_mercury_tests)
  add_executable(scalpel_color_tests Tests/ScalpelColorTests.cpp Source/SurgeryOnyx.h)
  target_compile_features(scalpel_color_tests PRIVATE cxx_std_20)
  add_test(NAME scalpel_color_five_knob_dsp COMMAND scalpel_color_tests)
  add_executable(scalpel_band_tests Tests/ScalpelBandTests.cpp Source/RootsAudioEngine.h)
  target_compile_features(scalpel_band_tests PRIVATE cxx_std_20)
  add_test(NAME scalpel_phase_limit_band_processing COMMAND scalpel_band_tests)
  add_executable(surgery_onyx_study Tests/OnyxStudy.cpp)
  target_compile_features(surgery_onyx_study PRIVATE cxx_std_20)
  add_test(NAME surgery_onyx_character_regression COMMAND surgery_onyx_study)
  enable_testing()
  configure_file("${juce_SOURCE_DIR}/examples/Assets/singing.ogg" "${CMAKE_BINARY_DIR}/surgery-test-music.ogg" COPYONLY)
  juce_add_console_app(prismvst_tests PRODUCT_NAME "PRISMVST Tests")
  target_sources(prismvst_tests PRIVATE
    Tests/ProcessorTests.cpp
    Tests/QualityTests.cpp
    Tests/SurgeryWeatherTests.cpp
    Tests/SurgeryWeatherFieldTests.cpp
    Tests/SurgeryStormChassisTests.cpp
    Tests/AllocationProbe.cpp
    Source/PluginProcessor.cpp
    Source/SurgeryWeatherAnalysis.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/RootsEditor.cpp
    Source/RootsEditor.h
    Source/ScalpelFluid3D.h
    Source/ScalpelFluid3D.cpp
    Source/RootsAudioEngine.h
    Source/SurgeryCarveEngine.h
    Source/SurgeryLookAndFeel.h
  )
  juce_generate_juce_header(prismvst_tests)
  target_link_libraries(prismvst_tests PRIVATE SurgeryRackAssets)
  target_compile_definitions(prismvst_tests PRIVATE
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
  )
  target_link_libraries(prismvst_tests PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
  )
  if(WIN32)
    target_link_libraries(prismvst_tests PRIVATE d3d11 dxgi d3dcompiler)
  endif()
  if(MSVC)
    target_link_options(prismvst_tests PRIVATE /STACK:8388608)
  endif()
  add_test(NAME prismvst_tests COMMAND prismvst_tests)
  add_test(NAME surgery_weather_calibration COMMAND prismvst_tests --weather-calibration)
  add_test(NAME surgery_weather_field COMMAND prismvst_tests --weather-field)
  # The legacy Surgery JUKE river renderer is no longer displayed by Scalpel.
  # Its historical diagnostic remains callable with --storm-chassis but is
  # excluded from Scalpel's active visual release regression suite.
  # Independent native C++20 ROOTS core; deliberately not linked into v1.2 DSP.
  add_executable(prism_roots_tests Tests/RootsCoreTests.cpp Source/PrismRootsCore.h)
  target_compile_features(prism_roots_tests PRIVATE cxx_std_20)
  add_test(NAME prism_roots_four_proofs COMMAND prism_roots_tests)
  add_executable(prism_lab_analyzer_tests Tests/PrismLabAnalyzerTests.cpp Source/PrismLabAnalyzer.h)
  target_compile_features(prism_lab_analyzer_tests PRIVATE cxx_std_20)
  add_test(NAME prism_lab_analyzer_four_proofs COMMAND prism_lab_analyzer_tests)
  add_executable(prism_roots_realtime_tests Tests/RootsRealtimeTests.cpp Source/RootsAudioEngine.h)
  target_compile_features(prism_roots_realtime_tests PRIVATE cxx_std_20)
  add_test(NAME prism_roots_realtime_four_proofs COMMAND prism_roots_realtime_tests)
  add_executable(prism_roots_ui_spectrum_tests Tests/RootsUiSpectrumTests.cpp Source/RootsSpectrumColours.h)
  target_compile_features(prism_roots_ui_spectrum_tests PRIVATE cxx_std_20)
  add_test(NAME prism_roots_ui_spectrum_four_proofs COMMAND prism_roots_ui_spectrum_tests)
  add_executable(prism_roots_parent_controls_tests Tests/RootsParentControlsTests.cpp Source/RootsAudioEngine.h)
  target_compile_features(prism_roots_parent_controls_tests PRIVATE cxx_std_20)
  add_test(NAME prism_roots_parent_controls_four_proofs COMMAND prism_roots_parent_controls_tests)
  add_executable(surgery_carve_tests Tests/SurgeryCarveTests.cpp Source/SurgeryCarveEngine.h)
  target_compile_features(surgery_carve_tests PRIVATE cxx_std_20)
  add_test(NAME surgery_carve_four_proofs COMMAND surgery_carve_tests)
  add_executable(surgery_sonar_tests Tests/SurgerySonarTests.cpp Source/SurgerySonar.h)
  target_compile_features(surgery_sonar_tests PRIVATE cxx_std_20)
  add_test(NAME surgery_sonar_four_proofs COMMAND surgery_sonar_tests)
  add_executable(surgery_header_layout_tests Tests/SurgeryHeaderLayoutTests.cpp Source/SurgeryHeaderLayout.h)
  target_compile_features(surgery_header_layout_tests PRIVATE cxx_std_20)
  add_test(NAME surgery_header_layout_proofs COMMAND surgery_header_layout_tests)
  add_executable(surgery_node_focus_tests Tests/SurgeryNodeFocusTests.cpp Source/SurgeryNodeFocus.h)
  target_compile_features(surgery_node_focus_tests PRIVATE cxx_std_20)
  add_test(NAME surgery_node_focus_proofs COMMAND surgery_node_focus_tests)
  add_executable(surgery_root_routing_tests Tests/SurgeryRootRoutingTests.cpp Source/RootsAudioEngine.h Source/SurgeryCarveEngine.h)
  target_compile_features(surgery_root_routing_tests PRIVATE cxx_std_20)
  add_test(NAME surgery_root_routing_proofs COMMAND surgery_root_routing_tests)
  add_executable(surgery_response_tests Tests/SurgeryResponseTests.cpp Source/SurgeryResponse.h)
  target_compile_features(surgery_response_tests PRIVATE cxx_std_20)
  add_test(NAME surgery_response_proofs COMMAND surgery_response_tests)
  add_executable(surgery_analysis_tests Tests/SurgeryAnalysisTests.cpp Source/SurgeryAnalysis.h)
  target_compile_features(surgery_analysis_tests PRIVATE cxx_std_20)
  add_test(NAME surgery_analysis_proofs COMMAND surgery_analysis_tests)
  add_executable(surgery_precision_tests Tests/SurgeryPrecisionTests.cpp Source/SurgeryPrecision.h)
  target_compile_features(surgery_precision_tests PRIVATE cxx_std_20)
  add_test(NAME surgery_precision_controls COMMAND surgery_precision_tests)
  add_executable(surgery_spectrogram_tests Tests/SurgerySpectrogramTests.cpp Source/SurgerySpectrogram.h)
  target_compile_features(surgery_spectrogram_tests PRIVATE cxx_std_20)
  add_test(NAME surgery_spectrogram_spsc COMMAND surgery_spectrogram_tests)
  add_executable(surgery_spectrum_fill_tests Tests/SurgerySpectrumFillTests.cpp Source/SurgerySpectrumFill.h)
  target_compile_features(surgery_spectrum_fill_tests PRIVATE cxx_std_20)
  add_test(NAME surgery_spectrum_fill_decay COMMAND surgery_spectrum_fill_tests)
  add_executable(surgery_spectral_colour_tests Tests/SurgerySpectralColoursTests.cpp Source/SurgerySpectralColours.h)
  target_compile_features(surgery_spectral_colour_tests PRIVATE cxx_std_20)
  add_test(NAME surgery_spectral_colour_mapping COMMAND surgery_spectral_colour_tests)
endif()

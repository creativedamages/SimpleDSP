cmake_minimum_required(VERSION 3.22)
project(SimpleDSP VERSION 0.4.1 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Bundle the MSVC runtime so the portable Windows executable can run without a separate redistributable.
if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

include(FetchContent)
FetchContent_Declare(juce
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 9.0.5
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(juce)

juce_add_gui_app(SimpleDSP
  PRODUCT_NAME "SimpleDSP"
  COMPANY_NAME "RobbyApps"
  BUNDLE_ID "com.robbyapps.htdsp"
)

# JUCE reads narrow C++ string literals as UTF-8. MSVC otherwise uses the
# machine's Windows code page for BOM-less .cpp/.h files, corrupting symbols
# such as bullet separators, disclosure arrows and degree symbols.
# /utf-8 sets both source and execution character sets to UTF-8.
if(MSVC)
  target_compile_options(SimpleDSP PRIVATE /utf-8)
endif()

target_sources(SimpleDSP PRIVATE Source/Main.cpp Source/DSP.h Source/Analysis.h Source/RemoteServer.h Source/RemotePage.h Source/ProjectSettings.h Source/TestSignal.h Source/SineSweep.h)
target_compile_definitions(SimpleDSP PRIVATE
  SIMPLEDSP_VERSION="${PROJECT_VERSION}"
  JUCE_ASIO=1
  JUCE_WASAPI=1
  JUCE_USE_WINDOWS_MIDI_SERVICES=0
  JUCE_USE_CURL=0
  JUCE_WEB_BROWSER=0
)
target_link_libraries(SimpleDSP PRIVATE
  juce::juce_audio_utils
  juce::juce_gui_extra
  juce::juce_recommended_config_flags
  juce::juce_recommended_warning_flags
  juce::juce_recommended_lto_flags
)
juce_generate_juce_header(SimpleDSP)

include(CTest)
if(BUILD_TESTING)
  add_executable(dual_dsp_tests tests/dsp_test.cpp)
  add_executable(dsp_analysis_tests tests/analysis_test.cpp)
  add_executable(dsp_preset_tests tests/preset_test.cpp)
  target_link_libraries(dsp_preset_tests PRIVATE juce::juce_core)
  target_compile_definitions(dsp_preset_tests PRIVATE JUCE_USE_CURL=0)
  foreach(test_target IN ITEMS dual_dsp_tests dsp_analysis_tests dsp_preset_tests)
    if(MSVC)
      target_compile_options(${test_target} PRIVATE /UNDEBUG)
    else()
      target_compile_options(${test_target} PRIVATE -UNDEBUG)
    endif()
  endforeach()
  add_test(NAME DSP_tests COMMAND dual_dsp_tests)
  add_test(NAME DSP_analysis_tests COMMAND dsp_analysis_tests)
  add_test(NAME DSP_preset_tests COMMAND dsp_preset_tests)
endif()

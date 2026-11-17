cmake_minimum_required(VERSION 3.22)
project(HTDSP VERSION 0.3.1 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(juce
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 9.0.5
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(juce)

juce_add_gui_app(HTDSP
  PRODUCT_NAME "HT DSP"
  COMPANY_NAME "RobbyApps"
  BUNDLE_ID "com.robbyapps.htdsp"
)

# JUCE reads narrow C++ string literals as UTF-8. MSVC otherwise uses the
# machine's Windows code page for BOM-less .cpp/.h files, corrupting symbols
# such as bullet separators, disclosure arrows and degree symbols.
# /utf-8 sets both source and execution character sets to UTF-8.
if(MSVC)
  target_compile_options(HTDSP PRIVATE /utf-8)
endif()

target_sources(HTDSP PRIVATE Source/Main.cpp Source/DSP.h Source/Analysis.h Source/RemoteServer.h Source/RemotePage.h Source/ProjectSettings.h Source/TestSignal.h Source/SineSweep.h)
target_compile_definitions(HTDSP PRIVATE
  JUCE_ASIO=1
  JUCE_WASAPI=1
  JUCE_USE_WINDOWS_MIDI_SERVICES=0
  JUCE_USE_CURL=0
  JUCE_WEB_BROWSER=0
)
target_link_libraries(HTDSP PRIVATE
  juce::juce_audio_utils
  juce::juce_gui_extra
  juce::juce_recommended_config_flags
  juce::juce_recommended_warning_flags
  juce::juce_recommended_lto_flags
)
juce_generate_juce_header(HTDSP)

include(CTest)
if(BUILD_TESTING)
  add_executable(dual_dsp_tests tests/dsp_test.cpp)
  add_executable(dsp_analysis_tests tests/analysis_test.cpp)
  foreach(test_target IN ITEMS dual_dsp_tests dsp_analysis_tests)
    if(MSVC)
      target_compile_options(${test_target} PRIVATE /UNDEBUG)
    else()
      target_compile_options(${test_target} PRIVATE -UNDEBUG)
    endif()
  endforeach()
  add_test(NAME DSP_tests COMMAND dual_dsp_tests)
  add_test(NAME DSP_analysis_tests COMMAND dsp_analysis_tests)
endif()

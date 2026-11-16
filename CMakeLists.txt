cmake_minimum_required(VERSION 3.22)
project(SimpleDSP VERSION 0.6.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(juce
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 9.0.5
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(juce)

juce_add_gui_app(SimpleDSP
  PRODUCT_NAME "Simple DSP"
  COMPANY_NAME "RobbyApps"
  ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon-512.png"
  ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon-192.png"
  BUNDLE_ID "com.robbyapps.simpledsp"
)

# Embed images in the EXE so the LAN web remote serves icons offline.
juce_add_binary_data(SimpleDSPResources
  HEADER_NAME SimpleDSPResources.h
  NAMESPACE SimpleDSPAssets
  SOURCES
    assets/favicon-32.png
    assets/apple-touch-icon.png
    assets/icon-192.png
    assets/icon-512.png
)

# JUCE reads narrow C++ string literals as UTF-8. MSVC otherwise uses the
# machine's Windows code page for BOM-less .cpp/.h files, corrupting symbols
# such as bullet separators, disclosure arrows and degree symbols.
# /utf-8 sets both source and execution character sets to UTF-8.
if(MSVC)
  target_compile_options(SimpleDSP PRIVATE /utf-8)
endif()

target_sources(SimpleDSP PRIVATE Source/Main.cpp Source/DSP.h Source/Analysis.h Source/RemoteServer.h Source/RemotePage.h)
target_compile_definitions(SimpleDSP PRIVATE
  JUCE_ASIO=1
  JUCE_WASAPI=1
  JUCE_USE_WINDOWS_MIDI_SERVICES=0
  JUCE_USE_CURL=0
  JUCE_WEB_BROWSER=0
)
target_link_libraries(SimpleDSP PRIVATE
  SimpleDSPResources
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
  add_test(NAME DSP_tests COMMAND dual_dsp_tests)
endif()

if(BUILD_TESTING)
  add_executable(dsp_analysis_tests tests/analysis_test.cpp)
  add_test(NAME DSP_analysis_tests COMMAND dsp_analysis_tests)
endif()

cmake_minimum_required(VERSION 3.22)
project(SimpleDSPPi VERSION 0.7.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(juce
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 9.0.5
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(juce)

# A console process, with ALSA audio and JUCE core/events only: no X11,
# window manager or desktop session is needed on Raspberry Pi OS Lite.
juce_add_console_app(SimpleDSPPi
  PRODUCT_NAME "Simple DSP Pi"
  COMPANY_NAME "RobbyApps"
)

juce_add_binary_data(SimpleDSPPiResources
  HEADER_NAME SimpleDSPResources.h
  NAMESPACE SimpleDSPAssets
  SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../assets/favicon-32.png
    ${CMAKE_CURRENT_SOURCE_DIR}/../assets/apple-touch-icon.png
    ${CMAKE_CURRENT_SOURCE_DIR}/../assets/icon-192.png
    ${CMAKE_CURRENT_SOURCE_DIR}/../assets/icon-512.png
)

target_sources(SimpleDSPPi PRIVATE
  Source/PiMain.cpp
  ../Source/DSP.h ../Source/Analysis.h
  ../Source/RemoteServer.h ../Source/RemotePage.h
)
target_include_directories(SimpleDSPPi PRIVATE ../Source)
target_compile_definitions(SimpleDSPPi PRIVATE
  JUCE_USE_CURL=0
  JUCE_ALSA=1
  JUCE_JACK=0
  JUCE_USE_X11=0
  JUCE_USE_WINDOWS_MIDI_SERVICES=0
)
target_link_libraries(SimpleDSPPi PRIVATE
  SimpleDSPPiResources
  juce::juce_audio_devices
  juce::juce_events
  juce::juce_core
  juce::juce_recommended_config_flags
  juce::juce_recommended_warning_flags
)
# Explicit baseline is ARMv8-A (Cortex-A72/Raspberry Pi 4); no
# -mcpu=native so runner-specific Armv9 instructions don't leak in.
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
  target_compile_options(SimpleDSPPi PRIVATE -march=armv8-a)
endif()
juce_generate_juce_header(SimpleDSPPi)

include(CTest)
if(BUILD_TESTING)
  add_executable(dsp_pi_core_tests ../tests/dsp_test.cpp)
  add_test(NAME Pi_DSP_Core COMMAND dsp_pi_core_tests)
  add_executable(dsp_pi_analysis_tests ../tests/analysis_test.cpp)
  add_test(NAME Pi_DSP_Analysis COMMAND dsp_pi_analysis_tests)
endif()

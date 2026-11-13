cmake_minimum_required(VERSION 3.22)
project(DualChannelDSP VERSION 0.2.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(juce
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 9.0.5
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(juce)

juce_add_gui_app(DualChannelDSP
  PRODUCT_NAME "Dual Channel DSP"
  COMPANY_NAME "RobbyApps"
  BUNDLE_ID "com.robbyapps.dualchanneldsp"
)

target_sources(DualChannelDSP PRIVATE Source/Main.cpp Source/DSP.h Source/RemoteServer.h Source/RemotePage.h)
target_compile_definitions(DualChannelDSP PRIVATE
  JUCE_ASIO=1
  JUCE_WASAPI=1
  JUCE_USE_WINDOWS_MIDI_SERVICES=0
  JUCE_USE_CURL=0
  JUCE_WEB_BROWSER=0
)
target_link_libraries(DualChannelDSP PRIVATE
  juce::juce_audio_utils
  juce::juce_gui_extra
  juce::juce_recommended_config_flags
  juce::juce_recommended_warning_flags
  juce::juce_recommended_lto_flags
)
juce_generate_juce_header(DualChannelDSP)

include(CTest)
if(BUILD_TESTING)
  add_executable(dual_dsp_tests tests/dsp_test.cpp)
  add_test(NAME DSP_tests COMMAND dual_dsp_tests)
endif()

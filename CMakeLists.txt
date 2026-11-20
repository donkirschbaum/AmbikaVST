cmake_minimum_required(VERSION 3.22)

# Universal binary (Apple Silicon + Intel) on macOS.
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures")
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")

project(AmbikaVST VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE: use a local checkout if JUCE_DIR is given, otherwise download it.
if(DEFINED JUCE_DIR)
  add_subdirectory(${JUCE_DIR} JUCE)
else()
  include(FetchContent)
  FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.15
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(JUCE)
endif()

set(AMBIKA_FORMATS VST3)
if(APPLE)
  list(APPEND AMBIKA_FORMATS AU)
endif()
list(APPEND AMBIKA_FORMATS Standalone)

juce_add_plugin(AmbikaVST
  PRODUCT_NAME "Ambika VST"
  COMPANY_NAME "Ambika Port"
  BUNDLE_ID "org.ambikaport.ambikavst"
  PLUGIN_MANUFACTURER_CODE Ambp
  PLUGIN_CODE Amb6
  FORMATS ${AMBIKA_FORMATS}
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE
  VST3_CATEGORIES Instrument Synth
  AU_MAIN_TYPE kAudioUnitType_MusicDevice
  DESCRIPTION "Emulation of the Ambika 6-voice hybrid polysynth")

target_sources(AmbikaVST PRIVATE
  src/engine/analog.cc
  src/engine/engine.cc
  src/engine/part.cc
  src/engine/voicecard.cc
  src/engine/resources_vc.cc
  src/engine/resources_ctl.cc
  src/plugin/Parameters.cpp
  src/plugin/PluginProcessor.cpp
  src/plugin/PluginEditor.cpp)

target_compile_definitions(AmbikaVST PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(AmbikaVST
  PRIVATE
    juce::juce_audio_utils
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags)

# Offline renderer used for regression tests (no JUCE needed).
add_executable(ambika_render_test EXCLUDE_FROM_ALL
  tools/render_test.cpp
  src/engine/analog.cc
  src/engine/engine.cc
  src/engine/part.cc
  src/engine/voicecard.cc
  src/engine/resources_vc.cc
  src/engine/resources_ctl.cc)

# Full plugin test: renders every factory preset (JUCE console app).
juce_add_console_app(ambika_preset_test PRODUCT_NAME "ambika_preset_test")
set_target_properties(ambika_preset_test PROPERTIES EXCLUDE_FROM_ALL TRUE)
target_sources(ambika_preset_test PRIVATE
  tools/preset_test.cpp
  src/engine/analog.cc
  src/engine/engine.cc
  src/engine/part.cc
  src/engine/voicecard.cc
  src/engine/resources_vc.cc
  src/engine/resources_ctl.cc
  src/plugin/Parameters.cpp
  src/plugin/PluginProcessor.cpp
  src/plugin/PluginEditor.cpp)
target_compile_definitions(ambika_preset_test PRIVATE
  JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="Ambika VST")
target_link_libraries(ambika_preset_test PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)

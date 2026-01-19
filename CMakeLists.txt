cmake_minimum_required(VERSION 3.22)
project(HammerSampler VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory($ENV{HOME}/JUCE JUCE)

juce_add_plugin(HammerSampler
    COMPANY_NAME "ALDENHammersmith"
    BUNDLE_ID "com.ALDENHammersmith.HammerSampler"
    PLUGIN_MANUFACTURER_CODE Alde
    PLUGIN_CODE Hsmr
    FORMATS Standalone VST3
    PRODUCT_NAME "Hammer Sampler"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
)

target_sources(HammerSampler PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/SamplerEngine.cpp
    Source/SamplerEngine.h
    Source/DiskStreaming.h
    Source/StreamingVoice.cpp
    Source/StreamingVoice.h
    Source/DiskStreamer.cpp
    Source/DiskStreamer.h
)

target_compile_definitions(HammerSampler PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(HammerSampler PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

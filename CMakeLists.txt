# AutoWarp - automatic warp / quantize plugin
# Created by DogSide Productions & Bluethedoggo
cmake_minimum_required(VERSION 3.22)

# Link the MSVC runtime statically so the plugin runs on any Windows PC
# without needing the "Visual C++ Redistributable" installed.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(AutoWarp VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- JUCE ---------------------------------------------------------------------
# Use a local JUCE checkout with  -DAUTOWARP_JUCE_PATH=C:/path/to/JUCE
# otherwise it is downloaded automatically.
set(AUTOWARP_JUCE_PATH "" CACHE PATH "Optional path to a local JUCE checkout")

if(AUTOWARP_JUCE_PATH)
    add_subdirectory(${AUTOWARP_JUCE_PATH} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.9
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# ---- Shared DogSide DSP core ---------------------------------------------------
# Both AutoWarp and the upcoming AutoTimeStretch target link this same library.
add_library(DogSideAudioCore STATIC
    Source/WarpEngine.cpp
    Source/WarpEngine.h
    Source/TempoEngine.cpp
    Source/TempoEngine.h)
target_include_directories(DogSideAudioCore PUBLIC Source)
target_compile_features(DogSideAudioCore PUBLIC cxx_std_17)
set_target_properties(DogSideAudioCore PROPERTIES POSITION_INDEPENDENT_CODE ON)

# ---- Plugin -------------------------------------------------------------------
juce_add_plugin(AutoWarp
    COMPANY_NAME                "DogSide Productions"
    COMPANY_WEBSITE             ""
    PLUGIN_MANUFACTURER_CODE    Dgsd
    PLUGIN_CODE                 Awrp
    PRODUCT_NAME                "AutoWarp"
    DESCRIPTION                 "Automatic warp and quantize by DogSide Productions and Bluethedoggo"
    BUNDLE_ID                   com.dogsideproductions.autowarp
    FORMATS                     VST3 Standalone
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    VST3_CATEGORIES             Instrument Sampler
    COPY_PLUGIN_AFTER_BUILD     FALSE)

target_sources(AutoWarp PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/WarpView.cpp
    Source/WarpView.h)

target_compile_definitions(AutoWarp PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(AutoWarp
    PRIVATE
        DogSideAudioCore
        juce::juce_audio_utils
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)


# ---- AutoTimeStretch: full-song tempo / duration processor -------------------
juce_add_plugin(AutoTimeStretch
    COMPANY_NAME                "DogSide Productions"
    COMPANY_WEBSITE             ""
    PLUGIN_MANUFACTURER_CODE    Dgsd
    PLUGIN_CODE                 Atsr
    PRODUCT_NAME                "AutoTimeStretch"
    DESCRIPTION                 "Full-song time stretching by DogSide Productions and Bluethedoggo"
    BUNDLE_ID                   com.dogsideproductions.autotimestretch
    FORMATS                     VST3 Standalone
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    VST3_CATEGORIES             Instrument Sampler
    COPY_PLUGIN_AFTER_BUILD     FALSE)

target_sources(AutoTimeStretch PRIVATE
    Source/AutoTimeStretchProcessor.cpp
    Source/AutoTimeStretchProcessor.h
    Source/AutoTimeStretchEditor.cpp
    Source/AutoTimeStretchEditor.h)

target_compile_definitions(AutoTimeStretch PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(AutoTimeStretch
    PRIVATE
        DogSideAudioCore
        juce::juce_audio_utils
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

cmake_minimum_required(VERSION 3.22)

project(Tstream VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Points at the JUCE SDK already downloaded on this machine. Override with
# -DTSTREAM_JUCE_DIR=... if JUCE ever moves.
#
# This replaces the old Projucer (.jucer) build. The generated
# Builds/VisualStudio2022 tree had absolute paths baked in from when the
# project lived at Desktop\Tstream, so it stopped building once the project
# moved - and regenerating it required the Projucer GUI. CMake keeps the
# whole build reproducible from the command line, matching TsVox.
set(TSTREAM_JUCE_DIR "C:/Users/troyo/Downloads/juce-9.0.0-windows/JUCE" CACHE PATH "Path to the JUCE SDK")
add_subdirectory(${TSTREAM_JUCE_DIR} JUCE)

juce_add_plugin(Tstream
    COMPANY_NAME "TroyDev"
    BUNDLE_ID "com.troydev.tstream"
    PLUGIN_MANUFACTURER_CODE Tdev
    PLUGIN_CODE NStr
    FORMATS VST3 Standalone
    PRODUCT_NAME "Tstream"
    DESCRIPTION "Stream audio over the network between DAW instances"
    VST3_CATEGORIES Fx
    COPY_PLUGIN_AFTER_BUILD TRUE
    VST3_COPY_DIR "C:/Users/troyo/AppData/Local/Programs/Common/VST3"
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_SYNTH FALSE)

juce_generate_juce_header(Tstream)

target_sources(Tstream
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/NetworkStreamer.cpp)

target_compile_definitions(Tstream
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        # Carried over from the .jucer's extraDefs.
        JUCE_IGNORE_VST3_MISMATCHED_PARAMETER_ID_WARNING=1
        # Carried over from the .jucer's JUCEOPTIONS. ASIO is off because it
        # needs the proprietary Steinberg ASIO SDK, which isn't on this machine.
        JUCE_WASAPI=1
        JUCE_DIRECTSOUND=1
        JUCE_ASIO=0
        JUCE_STRICT_REFCOUNTEDPOINTER=1)

target_link_libraries(Tstream
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

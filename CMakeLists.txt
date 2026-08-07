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
    # Compiled into the executable's icon resource, so Explorer, the taskbar
    # and any Desktop shortcut all pick it up with no extra install step.
    # Regenerate the source PNG with: python Assets/make_icon.py
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Assets/icon.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/Assets/icon.png"
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
        Source/NetworkStreamer.cpp
        Source/MonitorOutput.cpp
        Source/StandaloneApp.cpp)

target_compile_definitions(Tstream
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        # Swaps JUCE's default StandaloneFilterApp for the one in
        # Source/StandaloneApp.cpp, which adds the always-on-top pin to the
        # title bar and blanks the duplicate window caption. PUBLIC so the
        # definition reaches the Standalone wrapper target, not just SharedCode.
        JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1
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

# Copy the standalone somewhere stable. The build tree is the wrong home for an
# app the user launches daily from a shortcut: a `git clean` or a fresh CMake
# configure wipes Tstream_artefacts and silently breaks that shortcut. The VST3
# already gets this treatment via COPY_PLUGIN_AFTER_BUILD; this is the
# equivalent for the standalone.
#
# The auto-launch handshake is unaffected either way, since the standalone
# rewrites %APPDATA%\Tstream\receiver.path with its own location on every start.
if(WIN32)
    set(TSTREAM_STANDALONE_INSTALL_DIR "$ENV{LOCALAPPDATA}/Programs/Tstream"
        CACHE PATH "Where the standalone app is copied after building")

    if(TARGET Tstream_Standalone)
        add_custom_command(TARGET Tstream_Standalone POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${TSTREAM_STANDALONE_INSTALL_DIR}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:Tstream_Standalone>"
                    "${TSTREAM_STANDALONE_INSTALL_DIR}/Tstream.exe"
            COMMENT "Copying standalone to ${TSTREAM_STANDALONE_INSTALL_DIR}")
    endif()
endif()

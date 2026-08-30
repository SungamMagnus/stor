cmake_minimum_required(VERSION 3.22)

# Must precede project(): these drive the compiler checks.
#
# Build a universal binary. Left unset, CMake targets its OWN architecture — and
# an x86_64 CMake (Intel Homebrew under Rosetta) then silently produces an
# Intel-only plug-in, which native arm64 hosts skip without any error. arm64
# macOS starts at 11.0, so the deployment target has to clear that.
if(APPLE)
    if(NOT CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING
            "macOS build architectures" FORCE)
    endif()
    if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING
            "Minimum macOS version" FORCE)
    endif()
endif()

project(WhoompVST VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE: either an absolute checkout (-DJUCE_DIR=...) or the macOS default
# install location.
set(JUCE_DIR "/Applications/JUCE" CACHE PATH "Path to a JUCE source checkout")
if(NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "JUCE not found at ${JUCE_DIR} — pass -DJUCE_DIR=/path/to/JUCE")
endif()
add_subdirectory("${JUCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)

set(WHOOMP_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Parameters.cpp
    Source/WhoompEngine.cpp
    Source/Panel.cpp)

juce_add_plugin(Whoomp
    COMPANY_NAME            "Sungam"
    BUNDLE_ID               "com.sungam.whoomp"
    PLUGIN_MANUFACTURER_CODE Sngm
    PLUGIN_CODE             Whmp
    FORMATS                 VST3 AU Standalone
    PRODUCT_NAME            "Whoomp"
    DESCRIPTION             "Kick drum synthesiser"
    IS_SYNTH                TRUE
    NEEDS_MIDI_INPUT        TRUE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    VST3_CATEGORIES         Instrument Drum Synth
    AU_MAIN_TYPE            kAudioUnitType_MusicDevice)

target_sources(Whoomp PRIVATE ${WHOOMP_SOURCES})

target_compile_definitions(Whoomp PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_REPORT_APP_USAGE=0)

target_include_directories(Whoomp PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source")

target_link_libraries(Whoomp
    PRIVATE juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Dev tools: render the faceplate to a PNG, and run the real processor through
# a set of configurations without a host. -DWHOOMP_TOOLS=ON
option(WHOOMP_TOOLS "Build the offscreen renderer and the DSP checker" OFF)
if(WHOOMP_TOOLS)
    foreach(tool panel_shot dsp_check)
        juce_add_console_app(${tool} PRODUCT_NAME "${tool}" NEEDS_CURL FALSE)
        target_sources(${tool} PRIVATE tools/${tool}.cpp ${WHOOMP_SOURCES})
        target_compile_definitions(${tool} PRIVATE
            JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
            JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1)
        target_include_directories(${tool} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source")
        target_link_libraries(${tool} PRIVATE juce::juce_audio_utils)
    endforeach()
endif()

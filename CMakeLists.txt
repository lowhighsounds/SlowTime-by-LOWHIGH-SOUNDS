cmake_minimum_required(VERSION 3.24)
project(SlowTime VERSION 0.1.5 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# macOS: JUCE's Apple backends are Objective-C / Objective-C++ (.m/.mm), which
# need these languages enabled (they exist only on Apple toolchains, hence the
# guard). Without them the generate step fails with a missing CMAKE_C_COMPILE_OBJECT.
if(APPLE)
    enable_language(OBJC)
    enable_language(OBJCXX)
endif()

# Static MSVC runtime for EVERY target (JUCE modules, signalsmith, the plugin)
# so the shipped VST3 loads on machines without Visual Studio / the VC++
# redistributable. Must be set before the libraries are added so it applies
# consistently (mixing /MT and /MD would fail to link). Relies on CMP0091=NEW
# (default at cmake_minimum_required 3.24).
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

add_subdirectory(libs/JUCE)

# signalsmith-stretch ships its own CMakeLists.txt (INTERFACE target that
# FetchContent's its signalsmith-linear dependency automatically).
add_subdirectory(libs/signalsmith-stretch)

# Formats: VST3 everywhere; AU on macOS so Logic Pro / GarageBand load it.
# (CLAP was dropped -- its host-side latency handling caused glitches; VST3 is
# the priority format.) Pro Tools (AAX) is intentionally excluded (Avid-gated).
if(APPLE)
    set(SLOWTIME_FORMATS VST3 AU)
else()
    set(SLOWTIME_FORMATS VST3)
endif()

juce_add_plugin(SlowTime
    COMPANY_NAME "LowHigh Sounds"
    BUNDLE_ID "com.lowhighsounds.slowtime"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE LHss
    PLUGIN_CODE Sltm
    FORMATS ${SLOWTIME_FORMATS}
    VST3_CATEGORIES Fx PitchShift
    AU_MAIN_TYPE kAudioUnitType_Effect
    PRODUCT_NAME "SlowTime")

juce_generate_juce_header(SlowTime)

# Embedded fonts (Space Mono, SIL Open Font License -- see Resources/Fonts/OFL.txt)
# and brand/plugin logo art.
juce_add_binary_data(SlowTimeData SOURCES
    Resources/Fonts/SpaceMono-Regular.ttf
    Resources/Fonts/SpaceMono-Bold.ttf
    Resources/Logo/lowhigh-mark.png
    Resources/Logo/lowhigh-wordmark.png
    Resources/Logo/slowtime_logo.png)

target_sources(SlowTime PRIVATE
    Source/PluginProcessor.cpp
    Source/Parameters/ParameterLayout.cpp
    Source/DSP/StretchEngine.cpp
    Source/DSP/ChopVibeEngine.cpp
    Source/DSP/VibeModeMap.cpp
    Source/DSP/HostSync.cpp
    Source/PluginEditor.cpp
    Source/Editor/RetroLookAndFeel.cpp
    Source/Editor/WaveformDragDisplay.cpp)

target_compile_definitions(SlowTime PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(SlowTime PRIVATE
    SlowTimeData
    juce::juce_audio_utils
    juce::juce_dsp
    signalsmith-stretch
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

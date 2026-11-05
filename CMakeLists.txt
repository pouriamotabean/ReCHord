cmake_minimum_required(VERSION 3.22)
project(ReChord VERSION 2.2.0)
set(CMAKE_CXX_STANDARD 17)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.15)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(ReChord
    VERSION 2.2.0
    COMPANY_NAME "Pouria Motabean"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_MANUFACTURER_CODE Pmau
    PLUGIN_CODE Rchd
    FORMATS VST3
    PRODUCT_NAME "ReChord"
    VST3_CATEGORIES Instrument Synth
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(ReChord PRIVATE Source/PluginProcessor.cpp)

# fonts of the ReKit family (Inter, Space Mono - SIL Open Font License, see Assets/fonts/OFL-*.txt)
file(GLOB ASSET_FILES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Assets/fonts/*.ttf)
juce_add_binary_data(ReChordAssets SOURCES ${ASSET_FILES})
set_target_properties(ReChordAssets PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(ReChord PRIVATE ReChordAssets)

target_compile_definitions(ReChord PRIVATE HAS_ASSETS=1 RECHORD_VERSION=\"${PROJECT_VERSION}\")
target_compile_definitions(ReChord PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(ReChord
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_formats
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# engine test without JUCE: rechord_test out.wav [bpm] [voices]
add_executable(rechord_test tools/test_cli.cpp)

# UI snapshot / model test on Linux (not built on Windows CI): rechord_shot out.png
if (RECHORD_SHOT)
    juce_add_console_app(rechord_shot PRODUCT_NAME "rechord_shot")
    target_sources(rechord_shot PRIVATE tools/shot.cpp Source/PluginProcessor.cpp)
    target_compile_definitions(rechord_shot PRIVATE HAS_ASSETS=1 RECHORD_VERSION=\"${PROJECT_VERSION}\" JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(rechord_shot PRIVATE ReChordAssets juce::juce_audio_utils juce::juce_audio_formats juce::juce_recommended_config_flags)
endif()


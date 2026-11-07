cmake_minimum_required(VERSION 3.22)
project(BassDeamon VERSION 0.2.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(BASSDEAMON_BUILD_PLUGIN "Build the JUCE VST3" ON)
include(CTest)
add_library(bassdeamon_dsp INTERFACE)
target_include_directories(bassdeamon_dsp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/Source)
if(BUILD_TESTING)
    add_executable(dsp_tests tests/DspTests.cpp)
    target_link_libraries(dsp_tests PRIVATE bassdeamon_dsp)
    add_test(NAME dsp_contracts COMMAND dsp_tests)
endif()
if(BASSDEAMON_BUILD_PLUGIN)
    include(FetchContent)
    # Reproducible JUCE 9.0.5 revision; optional local checkout for offline builds.
    set(BASSDEAMON_JUCE_PATH "" CACHE PATH "Existing JUCE checkout")
    if(BASSDEAMON_JUCE_PATH)
        add_subdirectory(${BASSDEAMON_JUCE_PATH} ${CMAKE_BINARY_DIR}/JUCE)
    else()
        FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG d60b7a78030661574a615b586d9325ba7a7fa7dd GIT_SHALLOW FALSE)
        FetchContent_MakeAvailable(JUCE)
    endif()
    juce_add_plugin(BassDeamon
        COMPANY_NAME "Guy_BioRg" BUNDLE_ID "audio.guybiorg.bassdeamon"
        PLUGIN_MANUFACTURER_CODE Gbio PLUGIN_CODE BsDm
        FORMATS VST3 PRODUCT_NAME "BassDeamon"
        IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE VST3_CATEGORIES Fx Dynamics)
    target_sources(BassDeamon PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_compile_definitions(BassDeamon PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)
    target_link_libraries(BassDeamon PRIVATE bassdeamon_dsp juce::juce_audio_utils
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    if(BUILD_TESTING)
        juce_add_console_app(plugin_tests PRODUCT_NAME "BassDeamon plugin contracts")
        target_sources(plugin_tests PRIVATE tests/PluginTests.cpp tests/GuiCapture.cpp)
        set_target_properties(plugin_tests PROPERTIES OUTPUT_NAME plugin_tests)
        target_include_directories(plugin_tests PRIVATE Source)
        target_link_libraries(plugin_tests PRIVATE BassDeamon juce::juce_audio_utils)
        add_test(NAME plugin_contracts COMMAND plugin_tests)
    endif()
endif()

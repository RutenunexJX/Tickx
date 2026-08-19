cmake_minimum_required(VERSION 3.24)

project(WaveWorkbench VERSION 0.11.0 LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

option(WAVEWORKBENCH_BUILD_TESTS "Build automated tests" ON)
option(WAVEWORKBENCH_ENABLE_WELLEN "Build the Wellen FST reader" ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Svg Concurrent)

if(WAVEWORKBENCH_ENABLE_WELLEN)
    find_program(CARGO_EXECUTABLE
        NAMES cargo
        HINTS "$ENV{USERPROFILE}/.cargo/bin"
    )
    if(NOT CARGO_EXECUTABLE)
        message(FATAL_ERROR
            "WAVEWORKBENCH_ENABLE_WELLEN=ON requires the Rust cargo executable")
    endif()
    set(WAVE_WELLEN_READER_CARGO_OUTPUT
        "${CMAKE_BINARY_DIR}/cargo-target/release/wave-wellen-reader${CMAKE_EXECUTABLE_SUFFIX}")
    set(WAVE_WELLEN_READER_OUTPUT
        "${CMAKE_BINARY_DIR}/wave-wellen-reader${CMAKE_EXECUTABLE_SUFFIX}")
    add_custom_command(
        OUTPUT "${WAVE_WELLEN_READER_OUTPUT}"
        COMMAND "${CMAKE_COMMAND}" -E env
            "CARGO_TARGET_DIR=${CMAKE_BINARY_DIR}/cargo-target"
            "${CARGO_EXECUTABLE}" build --release --locked
            --manifest-path "${CMAKE_SOURCE_DIR}/tools/wellen_reader/Cargo.toml"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${WAVE_WELLEN_READER_CARGO_OUTPUT}"
            "${WAVE_WELLEN_READER_OUTPUT}"
        DEPENDS
            tools/wellen_reader/Cargo.toml
            tools/wellen_reader/Cargo.lock
            tools/wellen_reader/src/main.rs
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Building the Wellen on-demand FST reader"
        VERBATIM
    )
    add_custom_target(wave-wellen-reader ALL
        DEPENDS "${WAVE_WELLEN_READER_OUTPUT}")
endif()

function(wave_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

add_library(wavetime STATIC
    src/wavetime/src/time.cpp
    src/wavetime/src/timeline_viewport.cpp
)
target_include_directories(wavetime
    PUBLIC src/wavetime/include
)
wave_enable_warnings(wavetime)

add_library(wavecore STATIC
    src/wavecore/src/model.cpp
)
target_include_directories(wavecore
    PUBLIC src/wavecore/include
)
target_link_libraries(wavecore PUBLIC wavetime)
wave_enable_warnings(wavecore)

add_library(waveedit STATIC
    src/waveedit/src/commands.cpp
)
target_include_directories(waveedit
    PUBLIC src/waveedit/include
)
target_link_libraries(waveedit PUBLIC wavecore)
wave_enable_warnings(waveedit)

add_library(waveproject STATIC
    src/waveproject/src/project_io.cpp
)
target_include_directories(waveproject
    PUBLIC src/waveproject/include
)
target_link_libraries(waveproject PUBLIC wavecore Qt6::Core)
wave_enable_warnings(waveproject)

add_library(wavevalidate STATIC
    src/wavevalidate/src/condition.cpp
    src/wavevalidate/src/validation.cpp
)
target_include_directories(wavevalidate
    PUBLIC src/wavevalidate/include
)
target_link_libraries(wavevalidate PUBLIC wavecore)
wave_enable_warnings(wavevalidate)

add_library(wavetrace STATIC
    src/waveimport/src/trace.cpp
    src/waveimport/src/trace_hierarchy.cpp
)
target_include_directories(wavetrace
    PUBLIC src/waveimport/include
)
target_link_libraries(wavetrace PUBLIC wavecore)
wave_enable_warnings(wavetrace)

# Compatibility target for existing consumers. Trace parsing now has a
# dedicated ownership boundary without forcing downstream source changes.
add_library(waveimport INTERFACE)
target_link_libraries(waveimport INTERFACE wavetrace)

add_library(wavefst STATIC
    src/waveimport/src/fst_trace.cpp
)
target_include_directories(wavefst
    PUBLIC src/waveimport/include
)
target_link_libraries(wavefst PUBLIC wavetrace Qt6::Core)
wave_enable_warnings(wavefst)
if(WAVEWORKBENCH_ENABLE_WELLEN)
    add_dependencies(wavefst wave-wellen-reader)
endif()

add_library(wavecompare STATIC
    src/wavecompare/src/compare.cpp
    src/wavecompare/src/simulation_check.cpp
)
target_include_directories(wavecompare
    PUBLIC src/wavecompare/include
)
target_link_libraries(
    wavecompare PUBLIC wavecore waveimport wavevalidate waveintegration)
wave_enable_warnings(wavecompare)

add_library(waveintegration STATIC
    src/waveintegration/src/integration.cpp
    src/waveintegration/src/module_manifest.cpp
    src/waveintegration/src/simulation_source_navigation.cpp
    src/waveintegration/src/simulation_scenario_store.cpp
    src/waveintegration/src/stimulus_scenario.cpp
)
target_include_directories(waveintegration
    PUBLIC src/waveintegration/include
)
target_link_libraries(
    waveintegration PUBLIC wavecore waveimport wavevalidate Qt6::Core)
wave_enable_warnings(waveintegration)

add_library(wavesim STATIC
    src/wavesim/src/simulation_runner.cpp
    src/wavesim/src/simulation_build_cache.cpp
    src/wavesim/src/simulation_batch.cpp
    src/wavesim/src/simulation_pipeline.cpp
    src/wavesim/src/simulation_session.cpp
)
target_include_directories(wavesim
    PUBLIC src/wavesim/include
)
target_link_libraries(wavesim PUBLIC waveintegration waveimport waveproject Qt6::Core)
wave_enable_warnings(wavesim)

add_library(wavegenerate STATIC
    src/wavegenerate/src/generation.cpp
    src/wavegenerate/src/export.cpp
)
target_include_directories(wavegenerate
    PUBLIC src/wavegenerate/include
)
target_link_libraries(wavegenerate
    PUBLIC wavecore wavevalidate Qt6::Core Qt6::Gui Qt6::Svg
)
wave_enable_warnings(wavegenerate)

add_library(waveautomation STATIC
    src/waveautomation/src/automation.cpp
)
target_include_directories(waveautomation
    PUBLIC src/waveautomation/include
)
target_link_libraries(waveautomation
    PUBLIC wavecore wavetime waveedit wavevalidate Qt6::Core
)
wave_enable_warnings(waveautomation)

add_library(wavewidgets SHARED
    src/app/main_window.cpp
    src/app/main_window.h
    src/app/trace_canvas.cpp
    src/app/trace_canvas.h
    src/app/trace_signal_browser.cpp
    src/app/trace_signal_browser.h
    src/app/wave_canvas.cpp
    src/app/wave_canvas.h
    src/wavewidgets/src/widgets.cpp
    src/wavewidgets/include/wave/widgets.h
    src/wavewidgets/include/wave/widgets_export.h
)
target_include_directories(wavewidgets
    PUBLIC
        src/app
        src/wavewidgets/include
)
target_link_libraries(wavewidgets
    PUBLIC wavecore wavetime waveedit waveproject wavevalidate wavegenerate wavetrace wavefst wavecompare waveintegration wavesim
            Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent ${CMAKE_DL_LIBS}
)
if(WAVEWORKBENCH_ENABLE_WELLEN)
    add_dependencies(wavewidgets wave-wellen-reader)
endif()
set_target_properties(wavewidgets PROPERTIES
    PREFIX ""
    OUTPUT_NAME "wavewidgets"
    WINDOWS_EXPORT_ALL_SYMBOLS ON
)
wave_enable_warnings(wavewidgets)

add_executable(wave-workbench
    src/app/main.cpp
)
target_link_libraries(wave-workbench PRIVATE wavewidgets)
target_compile_definitions(wave-workbench
    PRIVATE WAVEWORKBENCH_VERSION="${PROJECT_VERSION}")
wave_enable_warnings(wave-workbench)

add_executable(wave-generate
    src/cli/generate_main.cpp
    src/cli/scenario_selection.h
)
target_link_libraries(wave-generate
    PRIVATE waveproject wavegenerate Qt6::Core Qt6::Gui
)
wave_enable_warnings(wave-generate)

add_executable(wave-compare
    src/cli/compare_main.cpp
    src/cli/scenario_selection.h
)
target_link_libraries(wave-compare
    PRIVATE waveproject waveimport wavefst wavecompare Qt6::Core
)
if(WAVEWORKBENCH_ENABLE_WELLEN)
    add_dependencies(wave-compare wave-wellen-reader)
endif()
wave_enable_warnings(wave-compare)

add_executable(wave-bridge
    src/cli/bridge_main.cpp
    src/cli/scenario_selection.h
)
target_link_libraries(wave-bridge
    PRIVATE waveproject waveintegration Qt6::Core
)
wave_enable_warnings(wave-bridge)

add_executable(wave-sim-runner
    src/cli/simulation_runner_main.cpp
)
target_link_libraries(wave-sim-runner PRIVATE wavesim Qt6::Core)
wave_enable_warnings(wave-sim-runner)

add_executable(wave-cli
    src/cli/wave_cli_main.cpp
    src/cli/scenario_selection.h
)
target_link_libraries(wave-cli
    PRIVATE waveautomation waveproject Qt6::Core
)
wave_enable_warnings(wave-cli)

set(WAVEWORKBENCH_PORTABLE_TARGETS
    wavewidgets
    wave-workbench
    wave-cli
    wave-generate
    wave-compare
    wave-bridge
    wave-sim-runner
)

configure_file(
    "${CMAKE_SOURCE_DIR}/docs/PACKAGE-README.txt.in"
    "${CMAKE_BINARY_DIR}/PACKAGE-README.txt"
    @ONLY
)

install(
    TARGETS ${WAVEWORKBENCH_PORTABLE_TARGETS}
    RUNTIME DESTINATION . COMPONENT Portable
    LIBRARY DESTINATION . COMPONENT Portable
    ARCHIVE DESTINATION lib COMPONENT Portable
)
install(
    FILES
        "${CMAKE_SOURCE_DIR}/README.md"
        "${CMAKE_BINARY_DIR}/PACKAGE-README.txt"
    DESTINATION .
    COMPONENT Portable
)
if(WAVEWORKBENCH_ENABLE_WELLEN)
    install(PROGRAMS "${WAVE_WELLEN_READER_OUTPUT}"
        DESTINATION .
        COMPONENT Portable)
endif()
install(
    FILES
        "${CMAKE_SOURCE_DIR}/docs/automation-cli.md"
        "${CMAKE_SOURCE_DIR}/docs/cli-quick-start.md"
        "${CMAKE_SOURCE_DIR}/docs/integration-contracts.md"
        "${CMAKE_SOURCE_DIR}/docs/simulation-runner.md"
        "${CMAKE_SOURCE_DIR}/docs/project-format.md"
        "${CMAKE_SOURCE_DIR}/docs/fst-reader.md"
    DESTINATION docs
    COMPONENT Portable
)
install(
    FILES
        "${CMAKE_SOURCE_DIR}/docs/examples/quick-start.operations.json"
    DESTINATION docs/examples
    COMPONENT Portable
)
install(
    DIRECTORY "${CMAKE_SOURCE_DIR}/src/waveautomation/schemas/automation/"
    DESTINATION schemas/automation
    COMPONENT Portable
)
install(
    DIRECTORY "${CMAKE_SOURCE_DIR}/src/waveintegration/schemas/stimulus/"
    DESTINATION schemas/stimulus
    COMPONENT Portable
)
install(
    DIRECTORY "${CMAKE_SOURCE_DIR}/examples/handshake/"
    DESTINATION examples/handshake
    COMPONENT Portable
)

if(WIN32)
    set_property(TARGET wave-workbench PROPERTY WIN32_EXECUTABLE FALSE)
endif()

if(WAVEWORKBENCH_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

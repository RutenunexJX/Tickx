cmake_minimum_required(VERSION 3.24)

project(WaveWorkbench VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

option(WAVEWORKBENCH_BUILD_TESTS "Build automated tests" ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Svg Concurrent)

function(wave_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

add_library(wavetime STATIC
    src/wavetime/src/time.cpp
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

add_library(waveimport STATIC
    src/waveimport/src/trace.cpp
)
target_include_directories(waveimport
    PUBLIC src/waveimport/include
)
target_link_libraries(waveimport PUBLIC wavecore)
wave_enable_warnings(waveimport)

add_library(wavecompare STATIC
    src/wavecompare/src/compare.cpp
)
target_include_directories(wavecompare
    PUBLIC src/wavecompare/include
)
target_link_libraries(wavecompare PUBLIC wavecore waveimport wavevalidate)
wave_enable_warnings(wavecompare)

add_library(waveintegration STATIC
    src/waveintegration/src/integration.cpp
    src/waveintegration/src/module_manifest.cpp
    src/waveintegration/src/stimulus_scenario.cpp
)
target_include_directories(waveintegration
    PUBLIC src/waveintegration/include
)
target_link_libraries(waveintegration PUBLIC wavecore Qt6::Core)
wave_enable_warnings(waveintegration)

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

add_executable(wave-workbench
    src/app/main.cpp
    src/app/main_window.cpp
    src/app/main_window.h
    src/app/trace_canvas.cpp
    src/app/trace_canvas.h
    src/app/wave_canvas.cpp
    src/app/wave_canvas.h
)
target_link_libraries(wave-workbench
    PRIVATE wavecore wavetime waveedit waveproject wavevalidate wavegenerate waveimport wavecompare waveintegration
            Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent
)
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
    PRIVATE waveproject waveimport wavecompare Qt6::Core
)
wave_enable_warnings(wave-compare)

add_executable(wave-bridge
    src/cli/bridge_main.cpp
    src/cli/scenario_selection.h
)
target_link_libraries(wave-bridge
    PRIVATE waveproject waveintegration Qt6::Core
)
wave_enable_warnings(wave-bridge)

add_executable(wave-cli
    src/cli/wave_cli_main.cpp
    src/cli/scenario_selection.h
)
target_link_libraries(wave-cli
    PRIVATE waveautomation waveproject Qt6::Core
)
wave_enable_warnings(wave-cli)

set(WAVEWORKBENCH_PORTABLE_TARGETS
    wave-workbench
    wave-cli
    wave-generate
    wave-compare
    wave-bridge
)

configure_file(
    "${CMAKE_SOURCE_DIR}/docs/PACKAGE-README.txt.in"
    "${CMAKE_BINARY_DIR}/PACKAGE-README.txt"
    @ONLY
)

install(
    TARGETS ${WAVEWORKBENCH_PORTABLE_TARGETS}
    RUNTIME DESTINATION .
    COMPONENT Portable
)
install(
    FILES
        "${CMAKE_SOURCE_DIR}/README.md"
        "${CMAKE_BINARY_DIR}/PACKAGE-README.txt"
    DESTINATION .
    COMPONENT Portable
)
install(
    FILES
        "${CMAKE_SOURCE_DIR}/docs/automation-cli.md"
        "${CMAKE_SOURCE_DIR}/docs/cli-quick-start.md"
        "${CMAKE_SOURCE_DIR}/docs/integration-contracts.md"
        "${CMAKE_SOURCE_DIR}/docs/project-format.md"
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

add_executable(wave-tests
    test_main.cpp
)
target_link_libraries(wave-tests
    PRIVATE wavetime wavecore waveedit waveproject wavevalidate wavegenerate waveimport wavecompare waveintegration
            Qt6::Core Qt6::Gui Qt6::Svg
)
wave_enable_warnings(wave-tests)
target_compile_definitions(wave-tests
    PRIVATE WAVE_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

add_test(NAME wave-core-tests COMMAND wave-tests)
add_test(NAME wave-app-smoke COMMAND wave-workbench --smoke-test)
add_test(
    NAME wave-example-smoke
    COMMAND wave-workbench --smoke-test "${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
)
add_test(
    NAME wave-autosave-smoke
    COMMAND wave-workbench
            "--autosave-smoke=${CMAKE_BINARY_DIR}/autosave-recovery.wave.json"
            "${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
)
add_test(
    NAME wave-lane-dialog-smoke
    COMMAND wave-workbench
            "--lane-dialog-screenshot=${CMAKE_BINARY_DIR}/lane-dialog-smoke.png"
            "${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
)
add_test(
    NAME wave-lane-removal-smoke
    COMMAND wave-workbench --lane-removal-smoke
)
add_test(
    NAME wave-lane-reorder-smoke
    COMMAND wave-workbench --lane-reorder-smoke
)
add_test(
    NAME wave-lane-autoscroll-smoke
    COMMAND wave-workbench
            "--lane-autoscroll-smoke=${CMAKE_BINARY_DIR}/lane-autoscroll-smoke.png"
            "${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
)
configure_file(
    "${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
    "${CMAKE_BINARY_DIR}/hidden-lane-smoke-project.wave.json"
    COPYONLY)
add_test(
    NAME wave-hidden-lane-smoke
    COMMAND wave-workbench
            "--hidden-lane-smoke=${CMAKE_BINARY_DIR}/hidden-lane-smoke-quick-hide.png"
            "${CMAKE_BINARY_DIR}/hidden-lane-smoke-project.wave.json"
)
add_test(
    NAME wave-group-header-smoke
    COMMAND wave-workbench
            --group-header-smoke
            "${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
)
add_test(
    NAME wave-signal-header-smoke
    COMMAND wave-workbench
            --signal-header-smoke
            "${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
)
add_test(
    NAME wave-canvas-add-lane-smoke
    COMMAND wave-workbench
            "--canvas-add-lane-smoke=${CMAKE_BINARY_DIR}/canvas-signal-management-smoke.png"
)
add_test(
    NAME wave-cursor-mode-smoke
    COMMAND wave-workbench
            "--cursor-mode-smoke=${CMAKE_BINARY_DIR}/cursor-mode-smoke.png"
)
add_test(
    NAME wave-wave-edit-smoke
    COMMAND wave-workbench
            "--wave-edit-smoke=${CMAKE_BINARY_DIR}/wave-edit-smoke.png"
)
add_test(
    NAME wave-wave-edit-autoscroll-smoke
    COMMAND wave-workbench
            "--wave-edit-autoscroll-smoke=${CMAKE_BINARY_DIR}/wave-edit-autoscroll-smoke.png"
            "${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
)
add_test(
    NAME wave-new-project-smoke
    COMMAND wave-workbench --new-project-smoke
)
add_test(
    NAME wave-user-journey-smoke
    COMMAND wave-workbench
            "--user-journey-smoke=${CMAKE_BINARY_DIR}/user-journey.wave.json"
)
add_test(
    NAME wave-edit-menu-smoke
    COMMAND wave-workbench
            "--edit-menu-screenshot=${CMAKE_BINARY_DIR}/edit-menu-smoke.png"
)
add_test(
    NAME wave-waveform-only-smoke
    COMMAND wave-workbench
            "--waveform-only-smoke=${CMAKE_BINARY_DIR}/waveform-only-smoke.png"
)
add_test(
    NAME wave-uri-smoke
    COMMAND wave-workbench --smoke-test
            "waveworkbench://compare?project=${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json&lane=lane-request&tick=80000"
)
add_test(
    NAME wave-generate-smoke
    COMMAND wave-generate
            "${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "${CMAKE_BINARY_DIR}/cli-generated"
)
add_test(
    NAME wave-compare-smoke
    COMMAND wave-compare
            "${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "${CMAKE_BINARY_DIR}/cli-compare"
)
add_test(
    NAME wave-bridge-describe-smoke
    COMMAND wave-bridge describe
            "${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "${CMAKE_BINARY_DIR}/bridge-workspace.json"
)
add_test(
    NAME wave-bridge-signals-smoke
    COMMAND wave-bridge import-signals
            "${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "${CMAKE_SOURCE_DIR}/examples/handshake/integration/zeroslack_signals.json"
            "${CMAKE_BINARY_DIR}/bridge-signals.wave.json"
)
add_test(
    NAME wave-bridge-frame-smoke
    COMMAND wave-bridge link-frame
            "${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "${CMAKE_SOURCE_DIR}/examples/handshake/integration/frame_sample_reference.json"
            "${CMAKE_BINARY_DIR}/bridge-frame.wave.json"
)
add_test(
    NAME wave-bridge-pinloom-smoke
    COMMAND wave-bridge pinloom-entry
            "${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "${CMAKE_SOURCE_DIR}/examples/handshake/exports"
            "${CMAKE_BINARY_DIR}/bridge-pinloom.json"
)

get_target_property(_wave_qt_core_location Qt6::Core IMPORTED_LOCATION)
get_filename_component(_wave_qt_bin_dir "${_wave_qt_core_location}" DIRECTORY)
get_filename_component(_wave_compiler_bin_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
set(_wave_test_environment "QT_QPA_PLATFORM=offscreen")
if(WIN32 AND EXISTS "C:/Windows/Fonts")
    list(APPEND _wave_test_environment "QT_QPA_FONTDIR=C:/Windows/Fonts")
endif()
set_tests_properties(
    wave-core-tests wave-app-smoke wave-example-smoke wave-autosave-smoke wave-lane-dialog-smoke wave-lane-removal-smoke wave-lane-reorder-smoke wave-lane-autoscroll-smoke wave-hidden-lane-smoke wave-group-header-smoke wave-signal-header-smoke wave-canvas-add-lane-smoke wave-cursor-mode-smoke wave-wave-edit-smoke wave-wave-edit-autoscroll-smoke wave-new-project-smoke wave-user-journey-smoke wave-edit-menu-smoke wave-waveform-only-smoke wave-uri-smoke wave-generate-smoke wave-compare-smoke
    wave-bridge-describe-smoke wave-bridge-signals-smoke wave-bridge-frame-smoke
    wave-bridge-pinloom-smoke
    PROPERTIES
    ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${_wave_qt_bin_dir};PATH=path_list_prepend:${_wave_compiler_bin_dir}"
)
set_tests_properties(
    wave-core-tests wave-app-smoke wave-example-smoke wave-autosave-smoke wave-lane-dialog-smoke wave-lane-removal-smoke wave-lane-reorder-smoke wave-lane-autoscroll-smoke wave-hidden-lane-smoke wave-group-header-smoke wave-signal-header-smoke wave-canvas-add-lane-smoke wave-cursor-mode-smoke wave-wave-edit-smoke wave-wave-edit-autoscroll-smoke wave-new-project-smoke wave-user-journey-smoke wave-edit-menu-smoke wave-waveform-only-smoke wave-uri-smoke wave-generate-smoke wave-compare-smoke
    wave-bridge-describe-smoke wave-bridge-signals-smoke wave-bridge-frame-smoke
    wave-bridge-pinloom-smoke
    PROPERTIES
    ENVIRONMENT "${_wave_test_environment}"
)
foreach(_wave_gui_test IN ITEMS
    wave-app-smoke wave-example-smoke wave-autosave-smoke wave-lane-dialog-smoke
    wave-lane-removal-smoke wave-lane-reorder-smoke wave-lane-autoscroll-smoke wave-hidden-lane-smoke wave-group-header-smoke wave-signal-header-smoke wave-canvas-add-lane-smoke
    wave-cursor-mode-smoke wave-wave-edit-smoke wave-wave-edit-autoscroll-smoke wave-new-project-smoke
    wave-user-journey-smoke wave-edit-menu-smoke wave-waveform-only-smoke wave-uri-smoke
)
    set_property(TEST ${_wave_gui_test} APPEND PROPERTY ENVIRONMENT
        "WAVEWORKBENCH_RECOVERY_DIR=${CMAKE_BINARY_DIR}/test-recovery/${_wave_gui_test}"
        "WAVEWORKBENCH_SETTINGS_DIR=${CMAKE_BINARY_DIR}/test-settings/${_wave_gui_test}")
endforeach()
set_tests_properties(wave-app-smoke wave-example-smoke wave-autosave-smoke wave-lane-dialog-smoke wave-lane-removal-smoke wave-lane-reorder-smoke wave-lane-autoscroll-smoke wave-hidden-lane-smoke wave-group-header-smoke wave-signal-header-smoke wave-canvas-add-lane-smoke wave-cursor-mode-smoke wave-wave-edit-smoke wave-wave-edit-autoscroll-smoke wave-new-project-smoke wave-user-journey-smoke wave-edit-menu-smoke wave-waveform-only-smoke wave-uri-smoke PROPERTIES
    TIMEOUT 20
)

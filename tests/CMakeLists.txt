add_executable(wave-tests
    test_main.cpp
)
target_link_libraries(wave-tests
    PRIVATE wavetime wavecore waveedit waveproject wavevalidate wavegenerate waveimport wavecompare waveintegration waveautomation
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
    NAME wave-file-conflict-smoke
    COMMAND wave-workbench
            "--file-conflict-smoke=${CMAKE_BINARY_DIR}/file-conflict-smoke.wave.json"
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
            "--group-header-smoke=${CMAKE_BINARY_DIR}/group-collapse-smoke.png"
            "${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
)
add_test(
    NAME wave-group-creation-smoke
    COMMAND wave-workbench
            "--group-creation-smoke=${CMAKE_BINARY_DIR}/group-creation-smoke.png"
            "${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
)
add_test(
    NAME wave-group-batch-smoke
    COMMAND wave-workbench
            "--group-batch-smoke=${CMAKE_BINARY_DIR}/group-batch-smoke.png"
            "${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
)
add_test(
    NAME wave-group-membership-smoke
    COMMAND wave-workbench
            "--group-membership-smoke=${CMAKE_BINARY_DIR}/group-membership-smoke.png"
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
            "waveworkbench://compare?project=${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json&scenario=scenario-handshake&lane=lane-request&tick=80000"
)
add_test(
    NAME wave-scenario-switch-smoke
    COMMAND wave-workbench
            "--scenario-switch-smoke=${CMAKE_BINARY_DIR}/scenario-switch-smoke.png"
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
add_test(
    NAME wave-cli-inspect-smoke
    COMMAND wave-cli inspect
            "${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            --scenario=scenario-handshake
)
add_test(
    NAME wave-cli-validate-smoke
    COMMAND wave-cli validate
            "${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            --scenario=scenario-handshake
)
add_test(
    NAME wave-cli-summary-smoke
    COMMAND wave-cli inspect
            "${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            --scenario=scenario-handshake
            --summary
)
add_test(
    NAME wave-cli-sample-smoke
    COMMAND wave-cli sample
            "${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "--at=80 ns"
            --scenario=scenario-handshake
            --lane=lane-request
            --lane=lane-data
)
add_test(
    NAME wave-cli-window-smoke
    COMMAND wave-cli window
            "${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "--start=70 ns"
            "--end=130 ns"
            --scenario=scenario-handshake
            --lane=lane-request
            --lane=lane-data
)
add_test(
    NAME wave-cli-apply-dry-run-smoke
    COMMAND wave-cli apply
            "${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "${CMAKE_SOURCE_DIR}/tests/fixtures/cli-operations.json"
            "--scenario=Request / acknowledge"
            --dry-run
)
add_test(
    NAME wave-cli-apply-smoke
    COMMAND wave-cli apply
            "${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "${CMAKE_SOURCE_DIR}/tests/fixtures/cli-operations.json"
            "--output=${CMAKE_BINARY_DIR}/cli-applied.wave.json"
)
add_test(
    NAME wave-cli-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DPROJECT=${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "-DOPERATIONS=${CMAKE_SOURCE_DIR}/tests/fixtures/cli-operations.json"
            "-DOUTPUT=${CMAKE_BINARY_DIR}/cli-contract-applied.wave.json"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_contract_smoke.cmake"
)
add_test(
    NAME wave-cli-advanced-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DPROJECT=${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "-DOPERATIONS=${CMAKE_SOURCE_DIR}/tests/fixtures/cli-operations-advanced.json"
            "-DOUTPUT=${CMAKE_BINARY_DIR}/cli-advanced-applied.wave.json"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_advanced_contract_smoke.cmake"
)
add_test(
    NAME wave-cli-window-range-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DPROJECT=${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "-DOPERATIONS=${CMAKE_SOURCE_DIR}/tests/fixtures/cli-operations-range.json"
            "-DCONFLICT_OPERATIONS=${CMAKE_SOURCE_DIR}/tests/fixtures/cli-operations-transfer-conflict.json"
            "-DASSERT_OPERATIONS=${CMAKE_SOURCE_DIR}/tests/fixtures/cli-operations-assert-fail.json"
            "-DOUTPUT=${CMAKE_BINARY_DIR}/cli-window-range-applied.wave.json"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_window_range_contract_smoke.cmake"
)
add_test(
    NAME wave-cli-new-dry-run-smoke
    COMMAND wave-cli new
            "${CMAKE_BINARY_DIR}/cli-new-direct-smoke"
            "--name=Direct smoke"
            "--duration=60 ns"
            --dry-run
)
add_test(
    NAME wave-cli-new-sequence-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DOPERATIONS=${CMAKE_SOURCE_DIR}/tests/fixtures/cli-new-operations.json"
            "-DINVALID_OPERATIONS=${CMAKE_SOURCE_DIR}/tests/fixtures/cli-new-operations-invalid.json"
            "-DOUTPUT=${CMAKE_BINARY_DIR}/cli-new-sequence-contract"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_new_sequence_contract_smoke.cmake"
)
add_test(
    NAME wave-cli-signals-smoke
    COMMAND wave-cli signals
            "${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "--match=req"
            --kind=bit
            --exact
)
add_test(
    NAME wave-cli-selector-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DPROJECT=${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "-DNAME_OPERATIONS=${CMAKE_SOURCE_DIR}/tests/fixtures/cli-new-operations-names.json"
            "-DNAME_EDIT_OPERATIONS=${CMAKE_SOURCE_DIR}/tests/fixtures/cli-name-edit-operations.json"
            "-DOUTPUT=${CMAKE_BINARY_DIR}/cli-selector-contract"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_selector_contract_smoke.cmake"
)
add_test(
    NAME wave-cli-symbolic-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DNEW_OPERATIONS=${CMAKE_SOURCE_DIR}/tests/fixtures/cli-symbolic-new-operations.json"
            "-DUPDATE_OPERATIONS=${CMAKE_SOURCE_DIR}/tests/fixtures/cli-symbolic-update-operations.json"
            "-DINVALID_OPERATIONS=${CMAKE_SOURCE_DIR}/tests/fixtures/cli-symbolic-invalid-map.json"
            "-DOUTPUT=${CMAKE_BINARY_DIR}/cli-symbolic-contract"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_symbolic_contract_smoke.cmake"
)
add_test(
    NAME wave-cli-intent-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DNEW_OPERATIONS=${CMAKE_SOURCE_DIR}/tests/fixtures/cli-symbolic-new-operations.json"
            "-DINTENT_OPERATIONS=${CMAKE_SOURCE_DIR}/tests/fixtures/cli-intent-operations.json"
            "-DCLEANUP_OPERATIONS=${CMAKE_SOURCE_DIR}/tests/fixtures/cli-intent-cleanup-operations.json"
            "-DINVALID_OPERATIONS=${CMAKE_SOURCE_DIR}/tests/fixtures/cli-intent-invalid-edge.json"
            "-DOUTPUT=${CMAKE_BINARY_DIR}/cli-intent-contract"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_intent_contract_smoke.cmake"
)
add_test(
    NAME wave-cli-capabilities-group-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DGROUP_OPERATIONS=${CMAKE_SOURCE_DIR}/tests/fixtures/cli-group-lifecycle-operations.json"
            "-DCLEANUP_OPERATIONS=${CMAKE_SOURCE_DIR}/tests/fixtures/cli-group-cleanup-operations.json"
            "-DINVALID_OPERATIONS=${CMAKE_SOURCE_DIR}/tests/fixtures/cli-group-invalid-operations.json"
            "-DOUTPUT=${CMAKE_BINARY_DIR}/cli-capabilities-group-contract"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_capabilities_group_contract_smoke.cmake"
)
add_test(
    NAME wave-cli-duplicate-signal-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DPROJECT=${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "-DOPERATIONS=${CMAKE_SOURCE_DIR}/tests/fixtures/cli-duplicate-signal-operations.json"
            "-DINVALID_OPERATIONS=${CMAKE_SOURCE_DIR}/tests/fixtures/cli-duplicate-signal-invalid.json"
            "-DOUTPUT=${CMAKE_BINARY_DIR}/cli-duplicate-signal-contract"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_duplicate_signal_contract_smoke.cmake"
)
add_test(
    NAME wave-cli-duration-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DPROJECT=${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "-DTRUNCATE_OPERATIONS=${CMAKE_SOURCE_DIR}/tests/fixtures/cli-duration-truncate-operations.json"
            "-DGUARDED_OPERATIONS=${CMAKE_SOURCE_DIR}/tests/fixtures/cli-duration-guarded-operations.json"
            "-DSAFE_OPERATIONS=${CMAKE_SOURCE_DIR}/tests/fixtures/cli-duration-safe-shrink-operations.json"
            "-DOUTPUT=${CMAKE_BINARY_DIR}/cli-duration-contract"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_duration_contract_smoke.cmake"
)
add_test(
    NAME wave-cli-edges-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DPROJECT=${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_edges_contract_smoke.cmake"
)
add_test(
    NAME wave-cli-relations-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DPROJECT=${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_relations_contract_smoke.cmake"
)
add_test(
    NAME wave-cli-relation-repair-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DPROJECT=${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "-DOUTPUT=${CMAKE_BINARY_DIR}/cli-relation-repair-contract"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_relation_repair_contract_smoke.cmake"
)
add_test(
    NAME wave-cli-relation-identity-repair-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DPROJECT=${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "-DOUTPUT=${CMAKE_BINARY_DIR}/cli-relation-identity-repair-contract"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_relation_identity_repair_contract_smoke.cmake"
)
add_test(
    NAME wave-cli-identity-validation-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DPROJECT=${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "-DOUTPUT=${CMAKE_BINARY_DIR}/cli-identity-validation-contract"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_identity_validation_contract_smoke.cmake"
)
add_test(
    NAME wave-cli-post-validation-guard-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DPROJECT=${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "-DOUTPUT=${CMAKE_BINARY_DIR}/cli-post-validation-guard-contract"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_post_validation_guard_contract_smoke.cmake"
)
add_test(
    NAME wave-cli-markers-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DPROJECT=${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_markers_contract_smoke.cmake"
)
add_test(
    NAME wave-cli-marker-repair-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DPROJECT=${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "-DOUTPUT=${CMAKE_BINARY_DIR}/cli-marker-repair-contract"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_marker_repair_contract_smoke.cmake"
)
add_test(
    NAME wave-cli-marker-validation-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DPROJECT=${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "-DOUTPUT=${CMAKE_BINARY_DIR}/cli-marker-validation-contract"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_marker_validation_contract_smoke.cmake"
)
add_test(
    NAME wave-cli-actionable-validation-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DPROJECT=${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "-DOUTPUT=${CMAKE_BINARY_DIR}/cli-actionable-validation-contract"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_actionable_validation_contract_smoke.cmake"
)
add_test(
    NAME wave-cli-relation-validation-context-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DPROJECT=${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "-DOUTPUT=${CMAKE_BINARY_DIR}/cli-relation-validation-context-contract"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_relation_validation_context_contract_smoke.cmake"
)
add_test(
    NAME wave-cli-waveform-validation-context-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DPROJECT=${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "-DOUTPUT=${CMAKE_BINARY_DIR}/cli-waveform-validation-context-contract"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_waveform_validation_context_contract_smoke.cmake"
)
add_test(
    NAME wave-cli-event-validation-context-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DPROJECT=${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "-DOUTPUT=${CMAKE_BINARY_DIR}/cli-event-validation-context-contract"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_event_validation_context_contract_smoke.cmake"
)
add_test(
    NAME wave-cli-event-link-repair-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DPROJECT=${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "-DOUTPUT=${CMAKE_BINARY_DIR}/cli-event-link-repair-contract"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_event_link_repair_contract_smoke.cmake"
)
add_test(
    NAME wave-cli-event-cycle-repair-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DPROJECT=${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "-DOUTPUT=${CMAKE_BINARY_DIR}/cli-event-cycle-repair-contract"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_event_cycle_repair_contract_smoke.cmake"
)
add_test(
    NAME wave-cli-event-clock-repair-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DPROJECT=${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "-DOUTPUT=${CMAKE_BINARY_DIR}/cli-event-clock-repair-contract"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_event_clock_repair_contract_smoke.cmake"
)
add_test(
    NAME wave-cli-lane-clock-repair-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DPROJECT=${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "-DOUTPUT=${CMAKE_BINARY_DIR}/cli-lane-clock-repair-contract"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_lane_clock_repair_contract_smoke.cmake"
)
add_test(
    NAME wave-cli-lane-group-repair-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DPROJECT=${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "-DOUTPUT=${CMAKE_BINARY_DIR}/cli-lane-group-repair-contract"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_lane_group_repair_contract_smoke.cmake"
)
add_test(
    NAME wave-cli-relation-clock-repair-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DPROJECT=${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "-DOUTPUT=${CMAKE_BINARY_DIR}/cli-relation-clock-repair-contract"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_relation_clock_repair_contract_smoke.cmake"
)
add_test(
    NAME wave-cli-trace-mapping-repair-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DPROJECT=${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "-DOUTPUT=${CMAKE_BINARY_DIR}/cli-trace-mapping-repair-contract"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_trace_mapping_repair_contract_smoke.cmake"
)
add_test(
    NAME wave-cli-trace-identity-compare-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DWAVE_COMPARE=$<TARGET_FILE:wave-compare>"
            "-DPROJECT=${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "-DOUTPUT=${CMAKE_BINARY_DIR}/cli-trace-identity-compare-contract"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_trace_identity_compare_contract_smoke.cmake"
)
add_test(
    NAME wave-cli-trace-reference-repair-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DWAVE_COMPARE=$<TARGET_FILE:wave-compare>"
            "-DPROJECT=${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "-DOUTPUT=${CMAKE_BINARY_DIR}/cli-trace-reference-repair-contract"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_trace_reference_repair_contract_smoke.cmake"
)
add_test(
    NAME wave-cli-scenario-selection-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DWAVE_GENERATE=$<TARGET_FILE:wave-generate>"
            "-DWAVE_COMPARE=$<TARGET_FILE:wave-compare>"
            "-DWAVE_BRIDGE=$<TARGET_FILE:wave-bridge>"
            "-DPROJECT=${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "-DSIGNALS=${CMAKE_SOURCE_DIR}/examples/handshake/integration/zeroslack_signals.json"
            "-DARTIFACT_DIR=${CMAKE_SOURCE_DIR}/examples/handshake/exports"
            "-DOUTPUT=${CMAKE_BINARY_DIR}/cli-scenario-selection-contract"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_scenario_selection_contract_smoke.cmake"
)
add_test(
    NAME wave-cli-compatibility-dispatch-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DPROJECT=${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "-DOPERATIONS=${CMAKE_SOURCE_DIR}/docs/examples/quick-start.operations.json"
            "-DOUTPUT=${CMAKE_BINARY_DIR}/cli-compatibility-dispatch"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_compatibility_dispatch_smoke.cmake"
)
add_test(
    NAME wave-portable-install-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DBUILD_DIRECTORY=${CMAKE_BINARY_DIR}"
            "-DCONFIGURATION=$<CONFIG>"
            "-DOUTPUT=${CMAKE_BINARY_DIR}/portable-install-contract"
            -P "${CMAKE_SOURCE_DIR}/tests/portable_install_contract_smoke.cmake"
)
if(WIN32)
    add_test(
        NAME wave-cli-powershell-stdin-smoke
        COMMAND powershell.exe -NoProfile -NonInteractive
                -ExecutionPolicy Bypass
                -File "${CMAKE_SOURCE_DIR}/tests/cli_powershell_stdin_smoke.ps1"
                -WaveCli "$<TARGET_FILE:wave-cli>"
                -Project "${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
                -Operations "${CMAKE_SOURCE_DIR}/tests/fixtures/cli-operations.json"
    )
endif()

get_target_property(_wave_qt_core_location Qt6::Core IMPORTED_LOCATION)
get_filename_component(_wave_qt_bin_dir "${_wave_qt_core_location}" DIRECTORY)
get_filename_component(_wave_compiler_bin_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
set(_wave_test_environment "QT_QPA_PLATFORM=offscreen")
if(WIN32 AND EXISTS "C:/Windows/Fonts")
    list(APPEND _wave_test_environment "QT_QPA_FONTDIR=C:/Windows/Fonts")
endif()
set_tests_properties(
    wave-core-tests wave-app-smoke wave-example-smoke wave-autosave-smoke wave-file-conflict-smoke wave-lane-dialog-smoke wave-lane-removal-smoke wave-lane-reorder-smoke wave-lane-autoscroll-smoke wave-hidden-lane-smoke wave-group-header-smoke wave-group-creation-smoke wave-group-batch-smoke wave-group-membership-smoke wave-signal-header-smoke wave-canvas-add-lane-smoke wave-cursor-mode-smoke wave-wave-edit-smoke wave-wave-edit-autoscroll-smoke wave-new-project-smoke wave-user-journey-smoke wave-edit-menu-smoke wave-waveform-only-smoke wave-uri-smoke wave-scenario-switch-smoke wave-generate-smoke wave-compare-smoke
    wave-bridge-describe-smoke wave-bridge-signals-smoke wave-bridge-frame-smoke
    wave-bridge-pinloom-smoke wave-cli-inspect-smoke wave-cli-validate-smoke
    wave-cli-summary-smoke wave-cli-sample-smoke wave-cli-window-smoke
    wave-cli-apply-dry-run-smoke wave-cli-apply-smoke wave-cli-contract-smoke
    wave-cli-advanced-contract-smoke wave-cli-window-range-contract-smoke
    wave-cli-new-dry-run-smoke wave-cli-new-sequence-contract-smoke
    wave-cli-signals-smoke wave-cli-selector-contract-smoke
    wave-cli-symbolic-contract-smoke wave-cli-intent-contract-smoke
    wave-cli-capabilities-group-contract-smoke
    wave-cli-duplicate-signal-contract-smoke
    wave-cli-duration-contract-smoke
    wave-cli-edges-contract-smoke
    wave-cli-markers-contract-smoke
    wave-cli-relations-contract-smoke
    wave-cli-relation-repair-contract-smoke
    wave-cli-relation-identity-repair-contract-smoke
    wave-cli-identity-validation-contract-smoke
    wave-cli-post-validation-guard-contract-smoke
    wave-cli-marker-repair-contract-smoke
    wave-cli-marker-validation-contract-smoke
    wave-cli-actionable-validation-contract-smoke
    wave-cli-relation-validation-context-contract-smoke
    wave-cli-waveform-validation-context-contract-smoke
    wave-cli-event-validation-context-contract-smoke
    wave-cli-event-link-repair-contract-smoke
    wave-cli-event-cycle-repair-contract-smoke
    wave-cli-event-clock-repair-contract-smoke
    wave-cli-lane-clock-repair-contract-smoke
    wave-cli-lane-group-repair-contract-smoke
    wave-cli-relation-clock-repair-contract-smoke
    wave-cli-trace-mapping-repair-contract-smoke
    wave-cli-trace-identity-compare-contract-smoke
    wave-cli-trace-reference-repair-contract-smoke
    wave-cli-scenario-selection-contract-smoke
    wave-cli-compatibility-dispatch-smoke
    wave-portable-install-contract-smoke
    PROPERTIES
    ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${_wave_qt_bin_dir};PATH=path_list_prepend:${_wave_compiler_bin_dir}"
)
set_tests_properties(
    wave-core-tests wave-app-smoke wave-example-smoke wave-autosave-smoke wave-file-conflict-smoke wave-lane-dialog-smoke wave-lane-removal-smoke wave-lane-reorder-smoke wave-lane-autoscroll-smoke wave-hidden-lane-smoke wave-group-header-smoke wave-group-creation-smoke wave-group-batch-smoke wave-group-membership-smoke wave-signal-header-smoke wave-canvas-add-lane-smoke wave-cursor-mode-smoke wave-wave-edit-smoke wave-wave-edit-autoscroll-smoke wave-new-project-smoke wave-user-journey-smoke wave-edit-menu-smoke wave-waveform-only-smoke wave-uri-smoke wave-scenario-switch-smoke wave-generate-smoke wave-compare-smoke
    wave-bridge-describe-smoke wave-bridge-signals-smoke wave-bridge-frame-smoke
    wave-bridge-pinloom-smoke wave-cli-inspect-smoke wave-cli-validate-smoke
    wave-cli-summary-smoke wave-cli-sample-smoke wave-cli-window-smoke
    wave-cli-apply-dry-run-smoke wave-cli-apply-smoke wave-cli-contract-smoke
    wave-cli-advanced-contract-smoke wave-cli-window-range-contract-smoke
    wave-cli-new-dry-run-smoke wave-cli-new-sequence-contract-smoke
    wave-cli-signals-smoke wave-cli-selector-contract-smoke
    wave-cli-symbolic-contract-smoke wave-cli-intent-contract-smoke
    wave-cli-capabilities-group-contract-smoke
    wave-cli-duplicate-signal-contract-smoke
    wave-cli-duration-contract-smoke
    wave-cli-edges-contract-smoke
    wave-cli-markers-contract-smoke
    wave-cli-relations-contract-smoke
    wave-cli-relation-repair-contract-smoke
    wave-cli-relation-identity-repair-contract-smoke
    wave-cli-identity-validation-contract-smoke
    wave-cli-post-validation-guard-contract-smoke
    wave-cli-marker-repair-contract-smoke
    wave-cli-marker-validation-contract-smoke
    wave-cli-actionable-validation-contract-smoke
    wave-cli-relation-validation-context-contract-smoke
    wave-cli-waveform-validation-context-contract-smoke
    wave-cli-event-validation-context-contract-smoke
    wave-cli-event-link-repair-contract-smoke
    wave-cli-event-cycle-repair-contract-smoke
    wave-cli-event-clock-repair-contract-smoke
    wave-cli-lane-clock-repair-contract-smoke
    wave-cli-lane-group-repair-contract-smoke
    wave-cli-relation-clock-repair-contract-smoke
    wave-cli-trace-mapping-repair-contract-smoke
    wave-cli-trace-identity-compare-contract-smoke
    wave-cli-trace-reference-repair-contract-smoke
    wave-cli-scenario-selection-contract-smoke
    wave-cli-compatibility-dispatch-smoke
    wave-portable-install-contract-smoke
    PROPERTIES
    ENVIRONMENT "${_wave_test_environment}"
)
foreach(_wave_gui_test IN ITEMS
    wave-app-smoke wave-example-smoke wave-autosave-smoke wave-file-conflict-smoke wave-lane-dialog-smoke
    wave-lane-removal-smoke wave-lane-reorder-smoke wave-lane-autoscroll-smoke wave-hidden-lane-smoke wave-group-header-smoke wave-group-creation-smoke wave-group-batch-smoke wave-group-membership-smoke wave-signal-header-smoke wave-canvas-add-lane-smoke
    wave-cursor-mode-smoke wave-wave-edit-smoke wave-wave-edit-autoscroll-smoke wave-new-project-smoke
    wave-user-journey-smoke wave-edit-menu-smoke wave-waveform-only-smoke wave-uri-smoke
    wave-scenario-switch-smoke
)
    set_property(TEST ${_wave_gui_test} APPEND PROPERTY ENVIRONMENT
        "WAVEWORKBENCH_RECOVERY_DIR=${CMAKE_BINARY_DIR}/test-recovery/${_wave_gui_test}"
        "WAVEWORKBENCH_SETTINGS_DIR=${CMAKE_BINARY_DIR}/test-settings/${_wave_gui_test}")
endforeach()
set_tests_properties(wave-app-smoke wave-example-smoke wave-autosave-smoke wave-file-conflict-smoke wave-lane-dialog-smoke wave-lane-removal-smoke wave-lane-reorder-smoke wave-lane-autoscroll-smoke wave-hidden-lane-smoke wave-group-header-smoke wave-group-creation-smoke wave-group-batch-smoke wave-group-membership-smoke wave-signal-header-smoke wave-canvas-add-lane-smoke wave-cursor-mode-smoke wave-wave-edit-smoke wave-wave-edit-autoscroll-smoke wave-new-project-smoke wave-user-journey-smoke wave-edit-menu-smoke wave-waveform-only-smoke wave-uri-smoke wave-scenario-switch-smoke PROPERTIES
    TIMEOUT 20
)
if(WIN32)
    set_tests_properties(
        wave-cli-powershell-stdin-smoke
        PROPERTIES
        ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${_wave_qt_bin_dir};PATH=path_list_prepend:${_wave_compiler_bin_dir}"
        ENVIRONMENT "${_wave_test_environment}"
    )
endif()

if(NOT DEFINED WAVE_CLI
   OR NOT DEFINED PROJECT
   OR NOT DEFINED OPERATIONS
   OR NOT DEFINED CONFLICT_OPERATIONS
   OR NOT DEFINED ASSERT_OPERATIONS
   OR NOT DEFINED OUTPUT)
    message(FATAL_ERROR "Window/range CLI contract smoke is missing an input")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" window "${PROJECT}"
            "--start=70 ns" "--end=130 ns"
            --scenario=scenario-handshake
            --lane=lane-request --lane=lane-data
    RESULT_VARIABLE window_code
    OUTPUT_VARIABLE window_json
    ERROR_VARIABLE window_error
)
if(NOT window_code EQUAL 0)
    message(FATAL_ERROR "window query failed (${window_code}): ${window_error}")
endif()
string(JSON source_sha GET "${window_json}" sourceSha256)
string(JSON window_start GET "${window_json}" startTick)
string(JSON window_end GET "${window_json}" endTick)
string(JSON lane_count GET "${window_json}" laneCount)
string(JSON event_count GET "${window_json}" eventCount)
string(JSON window_event_count GET "${window_json}" windowEventCount)
string(JSON relation_count GET "${window_json}" relationCount)
string(JSON marker_count GET "${window_json}" markerCount)
string(JSON request_start GET "${window_json}" lanes 0 startValue)
string(JSON request_end GET "${window_json}" lanes 0 endValue)
string(JSON data_clipped GET "${window_json}" lanes 1 segments 1 clipped)
string(JSON related_lane_selected GET "${window_json}" events 2 laneSelected)
if(NOT window_start STREQUAL "70000"
   OR NOT window_end STREQUAL "130000"
   OR NOT lane_count EQUAL 2
   OR NOT event_count EQUAL 3
   OR NOT window_event_count EQUAL 2
   OR NOT relation_count EQUAL 1
   OR NOT marker_count EQUAL 1
   OR NOT request_start STREQUAL "0"
   OR NOT request_end STREQUAL "1"
   OR NOT data_clipped
   OR related_lane_selected)
    message(FATAL_ERROR "window query omitted local waveform or dependency context")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" window "${PROJECT}"
            "--start=cycle 7" "--end=cycle 13"
            --scenario=scenario-handshake --lane=lane-request
    RESULT_VARIABLE cycle_window_code
    OUTPUT_VARIABLE cycle_window_json
    ERROR_VARIABLE cycle_window_error
)
if(NOT cycle_window_code EQUAL 0)
    message(FATAL_ERROR "cycle window failed (${cycle_window_code}): ${cycle_window_error}")
endif()
string(JSON cycle_window_start GET "${cycle_window_json}" startTick)
string(JSON cycle_window_end GET "${cycle_window_json}" endTick)
if(NOT cycle_window_start STREQUAL "70000"
   OR NOT cycle_window_end STREQUAL "130000")
    message(FATAL_ERROR "cycle window resolved to the wrong interval")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" apply "${PROJECT}" "${OPERATIONS}"
            --dry-run "--expect-sha256=${source_sha}"
    RESULT_VARIABLE dry_code
    OUTPUT_VARIABLE dry_json
    ERROR_VARIABLE dry_error
)
if(NOT dry_code EQUAL 0)
    message(FATAL_ERROR "range workflow dry-run failed (${dry_code}): ${dry_error}")
endif()
string(JSON dry_written GET "${dry_json}" written)
string(JSON dry_count GET "${dry_json}" operationCount)
string(JSON dry_sha GET "${dry_json}" resultSha256)
if(dry_written OR NOT dry_count EQUAL 6)
    message(FATAL_ERROR "range workflow dry-run contract is incorrect")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" apply "${PROJECT}" "${OPERATIONS}"
            "--output=${OUTPUT}" "--expect-sha256=${source_sha}"
    RESULT_VARIABLE apply_code
    OUTPUT_VARIABLE apply_json
    ERROR_VARIABLE apply_error
)
if(NOT apply_code EQUAL 0)
    message(FATAL_ERROR "range workflow apply failed (${apply_code}): ${apply_error}")
endif()
string(JSON result_sha GET "${apply_json}" resultSha256)
string(JSON written GET "${apply_json}" written)
if(NOT written OR NOT result_sha STREQUAL "${dry_sha}" OR NOT EXISTS "${OUTPUT}")
    message(FATAL_ERROR "range workflow output did not match its dry-run")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" sample "${OUTPUT}" "--at=55 ns"
            --lane=lane-request --lane=lane-ack
    RESULT_VARIABLE assignment_code
    OUTPUT_VARIABLE assignment_json
    ERROR_VARIABLE assignment_error
)
if(NOT assignment_code EQUAL 0)
    message(FATAL_ERROR "multi-lane assignment sample failed: ${assignment_error}")
endif()
string(JSON request_assigned GET "${assignment_json}" samples 0 value)
string(JSON ack_assigned GET "${assignment_json}" samples 1 value)
if(NOT request_assigned STREQUAL "1" OR NOT ack_assigned STREQUAL "1")
    message(FATAL_ERROR "multi-lane assignment did not update both signals")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" sample "${OUTPUT}" "--at=10 ns"
            --lane=lane-data-mirror --lane=lane-request-moved
    RESULT_VARIABLE transfer_code
    OUTPUT_VARIABLE transfer_json
    ERROR_VARIABLE transfer_error
)
if(NOT transfer_code EQUAL 0)
    message(FATAL_ERROR "transferred range sample failed: ${transfer_error}")
endif()
string(JSON mirrored_value GET "${transfer_json}" samples 0 value)
string(JSON moved_value GET "${transfer_json}" samples 1 value)
if(NOT mirrored_value STREQUAL "0x35" OR NOT moved_value STREQUAL "1")
    message(FATAL_ERROR "copied or moved range has the wrong value")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" sample "${OUTPUT}" "--at=90 ns" --lane=lane-request
    RESULT_VARIABLE cleared_code
    OUTPUT_VARIABLE cleared_json
    ERROR_VARIABLE cleared_error
)
if(NOT cleared_code EQUAL 0)
    message(FATAL_ERROR "moved source sample failed: ${cleared_error}")
endif()
string(JSON cleared_value GET "${cleared_json}" samples 0 value)
if(NOT cleared_value STREQUAL "0")
    message(FATAL_ERROR "move-range did not clear its source")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" apply "${PROJECT}" "${CONFLICT_OPERATIONS}" --dry-run
    RESULT_VARIABLE conflict_code
    OUTPUT_VARIABLE conflict_output
    ERROR_VARIABLE conflict_json
)
if(NOT conflict_code EQUAL 4)
    message(FATAL_ERROR "unsafe overwrite did not return exit code 4")
endif()
string(JSON conflict_operation GET "${conflict_json}" error operation)
string(JSON conflict_message GET "${conflict_json}" error message)
if(NOT conflict_operation EQUAL 0 OR NOT conflict_message MATCHES "overwrite")
    message(FATAL_ERROR "unsafe overwrite did not return a usable rejection")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" apply "${PROJECT}" "${ASSERT_OPERATIONS}" --dry-run
    RESULT_VARIABLE assertion_code
    OUTPUT_VARIABLE assertion_output
    ERROR_VARIABLE assertion_json
)
if(NOT assertion_code EQUAL 4)
    message(FATAL_ERROR "failed value assertion did not return exit code 4")
endif()
string(JSON assertion_operation GET "${assertion_json}" error operation)
string(JSON assertion_message GET "${assertion_json}" error message)
if(NOT assertion_operation EQUAL 1
   OR NOT assertion_message MATCHES "expected '0', actual '1'")
    message(FATAL_ERROR "failed assertion did not identify the value mismatch")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" validate "${OUTPUT}" --scenario=scenario-handshake
    RESULT_VARIABLE validate_code
    OUTPUT_VARIABLE validate_json
    ERROR_VARIABLE validate_error
)
if(NOT validate_code EQUAL 0)
    message(FATAL_ERROR "range workflow validation failed: ${validate_error}")
endif()
string(JSON validation_errors GET "${validate_json}" summary errors)
if(NOT validation_errors STREQUAL "0")
    message(FATAL_ERROR "range workflow output contains validation errors")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" inspect "${PROJECT}" --summary
    RESULT_VARIABLE final_code
    OUTPUT_VARIABLE final_json
    ERROR_VARIABLE final_error
)
if(NOT final_code EQUAL 0)
    message(FATAL_ERROR "final source inspect failed: ${final_error}")
endif()
string(JSON final_sha GET "${final_json}" sourceSha256)
if(NOT final_sha STREQUAL "${source_sha}")
    message(FATAL_ERROR "range workflow modified its source project")
endif()

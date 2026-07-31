if(NOT DEFINED WAVE_CLI
   OR NOT DEFINED OPERATIONS
   OR NOT DEFINED INVALID_OPERATIONS
   OR NOT DEFINED OUTPUT)
    message(FATAL_ERROR "New/sequence CLI contract smoke is missing an input")
endif()

set(actual_output "${OUTPUT}.wave.json")
set(second_output "${OUTPUT}-second.wave.json")
set(invalid_output_path "${OUTPUT}-invalid.wave.json")
file(REMOVE "${actual_output}" "${second_output}" "${invalid_output_path}")

execute_process(
    COMMAND "${WAVE_CLI}" new "${OUTPUT}"
            "--name=Sequence fixture"
            "--scenario-name=Stimulus"
            "--duration=60 ns"
            "--operations=${OPERATIONS}"
            --dry-run
    RESULT_VARIABLE dry_code
    OUTPUT_VARIABLE dry_json
    ERROR_VARIABLE dry_error
)
if(NOT dry_code EQUAL 0)
    message(FATAL_ERROR "new-project dry-run failed (${dry_code}): ${dry_error}")
endif()
string(JSON dry_written GET "${dry_json}" written)
string(JSON dry_output GET "${dry_json}" outputPath)
string(JSON dry_sha GET "${dry_json}" resultSha256)
string(JSON dry_project_id GET "${dry_json}" projectId)
string(JSON dry_scenario_id GET "${dry_json}" scenarioId)
string(JSON dry_operation_count GET "${dry_json}" operationCount)
string(JSON dry_duration GET "${dry_json}" project scenarios 0 durationTick)
string(JSON dry_lane_count GET "${dry_json}" project scenarios 0 laneCount)
string(JSON dry_step GET "${dry_json}" operations 3 stepTick)
string(JSON dry_cells GET "${dry_json}" operations 3 valueCellCount)
string(JSON dry_added_lanes LENGTH "${dry_json}" changes scenarios 0 addedLaneIds)
string(JSON dry_validation_errors GET "${dry_json}" validation summary errors)
string(JSON dry_project_created GET "${dry_json}" changes projectCreated)
string(JSON dry_scenario_created GET "${dry_json}" changes scenarioCreated)
if(dry_written
   OR NOT dry_output STREQUAL "${actual_output}"
   OR NOT dry_operation_count EQUAL 6
   OR NOT dry_duration STREQUAL "80000"
   OR NOT dry_lane_count EQUAL 3
   OR NOT dry_step STREQUAL "10000"
   OR NOT dry_cells STREQUAL "16"
   OR NOT dry_added_lanes EQUAL 3
   OR NOT dry_validation_errors STREQUAL "0"
   OR NOT dry_project_created
   OR NOT dry_scenario_created
   OR EXISTS "${actual_output}")
    message(FATAL_ERROR "new-project dry-run report is incorrect")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" new "${OUTPUT}"
            "--name=Sequence fixture"
            "--scenario-name=Stimulus"
            "--duration=60 ns"
            "--operations=${OPERATIONS}"
    RESULT_VARIABLE new_code
    OUTPUT_VARIABLE new_json
    ERROR_VARIABLE new_error
)
if(NOT new_code EQUAL 0)
    message(FATAL_ERROR "new-project write failed (${new_code}): ${new_error}")
endif()
string(JSON written GET "${new_json}" written)
string(JSON result_sha GET "${new_json}" resultSha256)
string(JSON project_id GET "${new_json}" projectId)
string(JSON scenario_id GET "${new_json}" scenarioId)
if(NOT written
   OR NOT EXISTS "${actual_output}"
   OR NOT result_sha STREQUAL "${dry_sha}"
   OR NOT project_id STREQUAL "${dry_project_id}"
   OR NOT scenario_id STREQUAL "${dry_scenario_id}")
    message(FATAL_ERROR "new-project output did not match its dry-run")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" sample "${actual_output}"
            "--at=cycle 1" --lane=lane-request
    RESULT_VARIABLE request_code
    OUTPUT_VARIABLE request_json
    ERROR_VARIABLE request_error
)
if(NOT request_code EQUAL 0)
    message(FATAL_ERROR "new request sample failed: ${request_error}")
endif()
string(JSON request_value GET "${request_json}" samples 0 value)
if(NOT request_value STREQUAL "1")
    message(FATAL_ERROR "new request sequence is incorrect")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" sample "${actual_output}"
            "--at=cycle 2" --lane=lane-data
    RESULT_VARIABLE data_code
    OUTPUT_VARIABLE data_json
    ERROR_VARIABLE data_error
)
if(NOT data_code EQUAL 0)
    message(FATAL_ERROR "new data sample failed: ${data_error}")
endif()
string(JSON data_value GET "${data_json}" samples 0 value)
if(NOT data_value STREQUAL "0x34")
    message(FATAL_ERROR "new data sequence is incorrect")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" window "${actual_output}"
            "--start=cycle 0" "--end=cycle 4"
            --lane=lane-request --lane=lane-data
    RESULT_VARIABLE window_code
    OUTPUT_VARIABLE window_json
    ERROR_VARIABLE window_error
)
if(NOT window_code EQUAL 0)
    message(FATAL_ERROR "new-project window failed: ${window_error}")
endif()
string(JSON window_lane_count GET "${window_json}" laneCount)
string(JSON window_start GET "${window_json}" startTick)
string(JSON window_end GET "${window_json}" endTick)
if(NOT window_lane_count EQUAL 2
   OR NOT window_start STREQUAL "0"
   OR NOT window_end STREQUAL "40000")
    message(FATAL_ERROR "new-project window interval is incorrect")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" validate "${actual_output}"
    RESULT_VARIABLE validate_code
    OUTPUT_VARIABLE validate_json
    ERROR_VARIABLE validate_error
)
if(NOT validate_code EQUAL 0)
    message(FATAL_ERROR "new project validation failed: ${validate_error}")
endif()
string(JSON validation_errors GET "${validate_json}" summary errors)
if(NOT validation_errors STREQUAL "0")
    message(FATAL_ERROR "new project contains validation errors")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" new "${OUTPUT}"
            "--name=Sequence fixture"
    RESULT_VARIABLE exists_code
    OUTPUT_VARIABLE exists_output
    ERROR_VARIABLE exists_json
)
if(NOT exists_code EQUAL 4)
    message(FATAL_ERROR "new replaced an existing output")
endif()
string(JSON exists_error GET "${exists_json}" error code)
if(NOT exists_error STREQUAL "output-exists")
    message(FATAL_ERROR "existing new-project output returned the wrong error")
endif()
execute_process(
    COMMAND "${WAVE_CLI}" inspect "${actual_output}" --summary
    RESULT_VARIABLE inspect_code
    OUTPUT_VARIABLE inspect_json
    ERROR_VARIABLE inspect_error
)
if(NOT inspect_code EQUAL 0)
    message(FATAL_ERROR "new output became unreadable: ${inspect_error}")
endif()
string(JSON persisted_sha GET "${inspect_json}" sourceSha256)
if(NOT persisted_sha STREQUAL "${result_sha}")
    message(FATAL_ERROR "existing-output rejection modified the project")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" new "${OUTPUT}-invalid"
            "--name=Invalid sequence"
            "--duration=60 ns"
            "--operations=${INVALID_OPERATIONS}"
    RESULT_VARIABLE invalid_code
    OUTPUT_VARIABLE invalid_stdout
    ERROR_VARIABLE invalid_json
)
if(NOT invalid_code EQUAL 4 OR EXISTS "${invalid_output_path}")
    message(FATAL_ERROR "invalid sequence created a partial project")
endif()
string(JSON invalid_operation GET "${invalid_json}" error operation)
if(NOT invalid_operation EQUAL 1 OR EXISTS "${invalid_output_path}")
    message(FATAL_ERROR "invalid sequence did not identify its failing operation")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" new "${OUTPUT}-second"
            "--name=Sequence fixture"
            "--scenario-name=Stimulus"
            "--duration=60 ns"
            "--operations=-"
            --dry-run
    INPUT_FILE "${OPERATIONS}"
    RESULT_VARIABLE second_code
    OUTPUT_VARIABLE second_json
    ERROR_VARIABLE second_error
)
if(NOT second_code EQUAL 0)
    message(FATAL_ERROR "second deterministic dry-run failed: ${second_error}")
endif()
string(JSON second_sha GET "${second_json}" resultSha256)
string(JSON second_project_id GET "${second_json}" projectId)
string(JSON second_scenario_id GET "${second_json}" scenarioId)
if(NOT second_sha STREQUAL "${dry_sha}"
   OR NOT second_project_id STREQUAL "${dry_project_id}"
   OR NOT second_scenario_id STREQUAL "${dry_scenario_id}")
    message(FATAL_ERROR "new-project identity or content is not deterministic")
endif()

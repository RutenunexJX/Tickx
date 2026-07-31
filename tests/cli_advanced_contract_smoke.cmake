if(NOT DEFINED WAVE_CLI OR NOT DEFINED PROJECT OR NOT DEFINED OPERATIONS OR NOT DEFINED OUTPUT)
    message(FATAL_ERROR "Advanced CLI contract smoke requires WAVE_CLI, PROJECT, OPERATIONS, and OUTPUT")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" inspect "${PROJECT}" --scenario=scenario-handshake --summary
    RESULT_VARIABLE summary_code
    OUTPUT_VARIABLE summary_json
    ERROR_VARIABLE summary_error
)
if(NOT summary_code EQUAL 0)
    message(FATAL_ERROR "summary inspect failed (${summary_code}): ${summary_error}")
endif()
string(JSON summary_detail GET "${summary_json}" detail)
string(JSON summary_lane_count GET "${summary_json}" project scenarios 0 laneCount)
if(NOT summary_detail STREQUAL "summary"
   OR NOT summary_lane_count EQUAL 8
   OR summary_json MATCHES "\"segments\""
   OR summary_json MATCHES "\"events\"")
    message(FATAL_ERROR "summary inspect retained or lost the wrong payload")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" sample "${PROJECT}" "--at=80 ns"
            --scenario=scenario-handshake --lane=lane-request --lane=lane-data
    RESULT_VARIABLE sample_code
    OUTPUT_VARIABLE sample_json
    ERROR_VARIABLE sample_error
)
if(NOT sample_code EQUAL 0)
    message(FATAL_ERROR "physical-time sample failed (${sample_code}): ${sample_error}")
endif()
string(JSON sample_tick GET "${sample_json}" atTick)
string(JSON sample_count GET "${sample_json}" sampleCount)
string(JSON request_value GET "${sample_json}" samples 0 value)
string(JSON data_value GET "${sample_json}" samples 1 value)
if(NOT sample_tick STREQUAL "80000"
   OR NOT sample_count EQUAL 2
   OR NOT request_value STREQUAL "1"
   OR NOT data_value STREQUAL "0x35")
    message(FATAL_ERROR "physical-time sample returned incorrect values")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" sample "${PROJECT}" "--at=cycle 8"
            --scenario=scenario-handshake --lane=lane-request
    RESULT_VARIABLE cycle_code
    OUTPUT_VARIABLE cycle_json
    ERROR_VARIABLE cycle_error
)
if(NOT cycle_code EQUAL 0)
    message(FATAL_ERROR "cycle sample failed (${cycle_code}): ${cycle_error}")
endif()
string(JSON cycle_tick GET "${cycle_json}" atTick)
if(NOT cycle_tick STREQUAL "80000")
    message(FATAL_ERROR "cycle sample resolved to the wrong tick")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" sample "${PROJECT}" "--at=1.5 tick"
    RESULT_VARIABLE invalid_time_code
    OUTPUT_VARIABLE invalid_time_output
    ERROR_VARIABLE invalid_time_json
)
if(NOT invalid_time_code EQUAL 4)
    message(FATAL_ERROR "fractional tick sample did not return exit code 4")
endif()
string(JSON invalid_time_error GET "${invalid_time_json}" error code)
if(NOT invalid_time_error STREQUAL "time-invalid")
    message(FATAL_ERROR "fractional tick sample did not return time-invalid JSON")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" apply "${PROJECT}" "${OPERATIONS}" --dry-run
    RESULT_VARIABLE dry_run_code
    OUTPUT_VARIABLE dry_run_json
    ERROR_VARIABLE dry_run_error
)
if(NOT dry_run_code EQUAL 0)
    message(FATAL_ERROR "advanced dry-run failed (${dry_run_code}): ${dry_run_error}")
endif()
string(JSON dry_run_sha GET "${dry_run_json}" resultSha256)
string(JSON dry_run_count GET "${dry_run_json}" operationCount)
if(NOT dry_run_count EQUAL 6)
    message(FATAL_ERROR "advanced dry-run reported the wrong operation count")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" apply "${PROJECT}" "${OPERATIONS}" "--output=${OUTPUT}"
    RESULT_VARIABLE apply_code
    OUTPUT_VARIABLE apply_json
    ERROR_VARIABLE apply_error
)
if(NOT apply_code EQUAL 0)
    message(FATAL_ERROR "advanced apply failed (${apply_code}): ${apply_error}")
endif()
string(JSON result_sha GET "${apply_json}" resultSha256)
if(NOT result_sha STREQUAL "${dry_run_sha}" OR NOT EXISTS "${OUTPUT}")
    message(FATAL_ERROR "advanced apply did not match dry-run")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" inspect "${OUTPUT}" --scenario=scenario-handshake --summary
    RESULT_VARIABLE result_code
    OUTPUT_VARIABLE result_json
    ERROR_VARIABLE result_error
)
if(NOT result_code EQUAL 0)
    message(FATAL_ERROR "advanced output inspect failed: ${result_error}")
endif()
string(JSON duration GET "${result_json}" project scenarios 0 durationTick)
string(JSON lane_count GET "${result_json}" project scenarios 0 laneCount)
string(JSON moved_lane_id GET "${result_json}" project scenarios 0 lanes 3 id)
string(JSON moved_lane_name GET "${result_json}" project scenarios 0 lanes 3 name)
string(JSON clock_period GET "${result_json}" project clockDomains 0 periodTick)
string(JSON clock_edge GET "${result_json}" project clockDomains 0 activeEdge)
if(NOT duration STREQUAL "230000"
   OR NOT lane_count EQUAL 7
   OR NOT moved_lane_id STREQUAL "lane-data"
   OR NOT moved_lane_name STREQUAL "payload"
   OR NOT clock_period STREQUAL "12000"
   OR NOT clock_edge STREQUAL "falling")
    message(FATAL_ERROR "advanced output contains incorrect signal or clock state")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" sample "${OUTPUT}" "--at=55 ns"
            --scenario=scenario-handshake --lane=lane-request
    RESULT_VARIABLE result_sample_code
    OUTPUT_VARIABLE result_sample_json
    ERROR_VARIABLE result_sample_error
)
if(NOT result_sample_code EQUAL 0)
    message(FATAL_ERROR "advanced result sample failed: ${result_sample_error}")
endif()
string(JSON result_request GET "${result_sample_json}" samples 0 value)
if(NOT result_request STREQUAL "1")
    message(FATAL_ERROR "physical/cycle range edit is absent from advanced output")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" validate "${OUTPUT}" --scenario=scenario-handshake
    RESULT_VARIABLE validate_code
    OUTPUT_VARIABLE validate_json
    ERROR_VARIABLE validate_error
)
if(NOT validate_code EQUAL 0)
    message(FATAL_ERROR "advanced output validation failed: ${validate_error}")
endif()
string(JSON validation_errors GET "${validate_json}" summary errors)
if(NOT validation_errors STREQUAL "0")
    message(FATAL_ERROR "advanced output contains validation errors")
endif()

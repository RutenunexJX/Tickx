if(NOT DEFINED WAVE_CLI
   OR NOT DEFINED PROJECT
   OR NOT DEFINED NAME_OPERATIONS
   OR NOT DEFINED NAME_EDIT_OPERATIONS
   OR NOT DEFINED OUTPUT)
    message(FATAL_ERROR "Selector CLI contract smoke is missing an input")
endif()

set(actual_output "${OUTPUT}.wave.json")
set(second_output "${OUTPUT}-second.wave.json")
set(edit_output "${OUTPUT}-name-edit.wave.json")
file(REMOVE "${actual_output}" "${second_output}" "${edit_output}")

execute_process(
    COMMAND "${WAVE_CLI}" signals "${PROJECT}"
            "--match=req" --kind=bit --exact
    RESULT_VARIABLE signals_code
    OUTPUT_VARIABLE signals_json
    ERROR_VARIABLE signals_error
)
if(NOT signals_code EQUAL 0)
    message(FATAL_ERROR "signal lookup failed: ${signals_error}")
endif()
string(JSON signal_matches GET "${signals_json}" matchCount)
string(JSON signal_returned GET "${signals_json}" returnedCount)
string(JSON signal_id GET "${signals_json}" signals 0 id)
string(JSON signal_has_segments
       ERROR_VARIABLE signal_segments_error
       GET "${signals_json}" signals 0 segments)
if(NOT signal_matches EQUAL 1
   OR NOT signal_returned EQUAL 1
   OR NOT signal_id STREQUAL "lane-request"
   OR NOT signal_segments_error)
    message(FATAL_ERROR "signal lookup returned a non-compact or wrong result")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" signals "${PROJECT}" --limit=2
    RESULT_VARIABLE limited_code
    OUTPUT_VARIABLE limited_json
    ERROR_VARIABLE limited_error
)
if(NOT limited_code EQUAL 0)
    message(FATAL_ERROR "limited signal lookup failed: ${limited_error}")
endif()
string(JSON limited_matches GET "${limited_json}" matchCount)
string(JSON limited_returned GET "${limited_json}" returnedCount)
string(JSON limited_truncated GET "${limited_json}" truncated)
if(NOT limited_matches EQUAL 8
   OR NOT limited_returned EQUAL 2
   OR NOT limited_truncated)
    message(FATAL_ERROR "signal lookup limit did not report truncation")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" signals "${PROJECT}" --exact
    RESULT_VARIABLE exact_without_match_code
    OUTPUT_VARIABLE exact_without_match_output
    ERROR_VARIABLE exact_without_match_json
)
if(NOT exact_without_match_code EQUAL 2)
    message(FATAL_ERROR "--exact without --match was accepted")
endif()
string(JSON exact_without_match_error
       GET "${exact_without_match_json}" error code)
if(NOT exact_without_match_error STREQUAL "usage")
    message(FATAL_ERROR "--exact without --match returned the wrong error")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" apply "${PROJECT}" "${NAME_EDIT_OPERATIONS}"
            --dry-run "--scenario=request / ACKNOWLEDGE"
    RESULT_VARIABLE edit_dry_run_code
    OUTPUT_VARIABLE edit_dry_run_json
    ERROR_VARIABLE edit_dry_run_error
)
if(NOT edit_dry_run_code EQUAL 0 OR EXISTS "${edit_output}")
    message(FATAL_ERROR "name-addressed edit dry-run failed: ${edit_dry_run_error}")
endif()
string(JSON edit_dry_run_sha GET "${edit_dry_run_json}" resultSha256)
string(JSON edit_assert_lane GET "${edit_dry_run_json}" operations 0 laneId)
string(JSON edit_first_lane GET "${edit_dry_run_json}" operations 1 laneIds 0)
string(JSON edit_second_lane GET "${edit_dry_run_json}" operations 1 laneIds 1)
if(NOT edit_assert_lane STREQUAL "lane-request"
   OR NOT edit_first_lane STREQUAL "lane-ack"
   OR NOT edit_second_lane STREQUAL "lane-data")
    message(FATAL_ERROR "edit report did not return canonical Lane IDs")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" apply "${PROJECT}" "${NAME_EDIT_OPERATIONS}"
            "--output=${edit_output}" "--scenario=Request / acknowledge"
    RESULT_VARIABLE edit_code
    OUTPUT_VARIABLE edit_json
    ERROR_VARIABLE edit_error
)
if(NOT edit_code EQUAL 0 OR NOT EXISTS "${edit_output}")
    message(FATAL_ERROR "name-addressed edit failed: ${edit_error}")
endif()
string(JSON edit_sha GET "${edit_json}" resultSha256)
if(NOT edit_sha STREQUAL "${edit_dry_run_sha}")
    message(FATAL_ERROR "name-addressed dry-run and write SHA differ")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" sample "${edit_output}"
            "--lane=ack" "--lane=DATA[7:0]"
            "--clock=clk" "--at=cycle 8"
    RESULT_VARIABLE edit_sample_code
    OUTPUT_VARIABLE edit_sample_json
    ERROR_VARIABLE edit_sample_error
)
if(NOT edit_sample_code EQUAL 0)
    message(FATAL_ERROR "name-addressed edit sample failed: ${edit_sample_error}")
endif()
string(JSON edit_ack_value GET "${edit_sample_json}" samples 0 value)
string(JSON edit_data_value GET "${edit_sample_json}" samples 1 value)
if(NOT edit_ack_value STREQUAL "1"
   OR NOT edit_data_value STREQUAL "0xa5")
    message(FATAL_ERROR "name-addressed edit values are incorrect")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" sample "${PROJECT}"
            "--scenario=Request / acknowledge"
            "--clock=CLK" "--lane=REQ" "--at=cycle 8"
    RESULT_VARIABLE sample_code
    OUTPUT_VARIABLE sample_json
    ERROR_VARIABLE sample_error
)
if(NOT sample_code EQUAL 0)
    message(FATAL_ERROR "name-addressed sample failed: ${sample_error}")
endif()
string(JSON sample_scenario GET "${sample_json}" scenarioId)
string(JSON sample_clock GET "${sample_json}" clockId)
string(JSON sample_lane GET "${sample_json}" samples 0 laneId)
string(JSON sample_value GET "${sample_json}" samples 0 value)
if(NOT sample_scenario STREQUAL "scenario-handshake"
   OR NOT sample_clock STREQUAL "clock-main"
   OR NOT sample_lane STREQUAL "lane-request"
   OR NOT sample_value STREQUAL "1")
    message(FATAL_ERROR "name selectors did not resolve to canonical IDs")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" window "${PROJECT}"
            "--lane=req" "--lane=data[7:0]"
            "--start=cycle 7" "--end=cycle 13"
    RESULT_VARIABLE window_code
    OUTPUT_VARIABLE window_json
    ERROR_VARIABLE window_error
)
if(NOT window_code EQUAL 0)
    message(FATAL_ERROR "name-addressed window failed: ${window_error}")
endif()
string(JSON window_lane_count GET "${window_json}" laneCount)
string(JSON window_first_id GET "${window_json}" lanes 0 id)
string(JSON window_second_id GET "${window_json}" lanes 1 id)
if(NOT window_lane_count EQUAL 2
   OR NOT window_first_id STREQUAL "lane-request"
   OR NOT window_second_id STREQUAL "lane-data")
    message(FATAL_ERROR "window did not retain canonical lane order")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" new "${OUTPUT}"
            "--name=Name selector fixture"
            "--duration=40 ns"
            "--operations=${NAME_OPERATIONS}"
    RESULT_VARIABLE new_code
    OUTPUT_VARIABLE new_json
    ERROR_VARIABLE new_error
)
if(NOT new_code EQUAL 0)
    message(FATAL_ERROR "name-addressed new failed: ${new_error}")
endif()
string(JSON new_written GET "${new_json}" written)
string(JSON new_sha GET "${new_json}" resultSha256)
string(JSON new_operation_count GET "${new_json}" operationCount)
string(JSON generated_clock_id GET "${new_json}" operations 0 clockId)
string(JSON generated_request_id GET "${new_json}" operations 1 laneId)
string(JSON sequence_request_id GET "${new_json}" operations 3 laneIds 0)
if(NOT new_written
   OR NOT EXISTS "${actual_output}"
   OR NOT new_operation_count EQUAL 6
   OR generated_clock_id STREQUAL "clk"
   OR generated_request_id STREQUAL "request"
   OR NOT sequence_request_id STREQUAL "${generated_request_id}")
    message(FATAL_ERROR "same-batch names were not canonicalized")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" signals "${actual_output}"
            "--match=DATA" --kind=bus --exact
    RESULT_VARIABLE new_signals_code
    OUTPUT_VARIABLE new_signals_json
    ERROR_VARIABLE new_signals_error
)
if(NOT new_signals_code EQUAL 0)
    message(FATAL_ERROR "new-project signal lookup failed: ${new_signals_error}")
endif()
string(JSON new_data_matches GET "${new_signals_json}" matchCount)
if(NOT new_data_matches EQUAL 1)
    message(FATAL_ERROR "new-project Bus was not discoverable by name")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" sample "${actual_output}"
            "--lane=request" "--at=cycle 3"
    RESULT_VARIABLE request_code
    OUTPUT_VARIABLE request_json
    ERROR_VARIABLE request_error
)
if(NOT request_code EQUAL 0)
    message(FATAL_ERROR "new-project request sample failed: ${request_error}")
endif()
string(JSON request_value GET "${request_json}" samples 0 value)
if(NOT request_value STREQUAL "1")
    message(FATAL_ERROR "name-addressed request sequence is incorrect")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" new "${OUTPUT}-second"
            "--name=Name selector fixture"
            "--duration=40 ns"
            "--operations=${NAME_OPERATIONS}"
            --dry-run
    RESULT_VARIABLE second_code
    OUTPUT_VARIABLE second_json
    ERROR_VARIABLE second_error
)
if(NOT second_code EQUAL 0)
    message(FATAL_ERROR "second name-addressed dry-run failed: ${second_error}")
endif()
string(JSON second_sha GET "${second_json}" resultSha256)
if(NOT second_sha STREQUAL "${new_sha}"
   OR EXISTS "${second_output}")
    message(FATAL_ERROR "name-addressed creation is not deterministic")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" sample "${PROJECT}"
            "--lane=missing-signal" "--at=0 ns"
    RESULT_VARIABLE missing_code
    OUTPUT_VARIABLE missing_output
    ERROR_VARIABLE missing_json
)
if(NOT missing_code EQUAL 4)
    message(FATAL_ERROR "missing signal selector was accepted")
endif()
string(JSON missing_error GET "${missing_json}" error code)
if(NOT missing_error STREQUAL "selector-invalid")
    message(FATAL_ERROR "missing selector returned the wrong error")
endif()

if(NOT DEFINED WAVE_CLI
   OR NOT DEFINED NEW_OPERATIONS
   OR NOT DEFINED UPDATE_OPERATIONS
   OR NOT DEFINED INVALID_OPERATIONS
   OR NOT DEFINED OUTPUT)
    message(FATAL_ERROR "Symbolic CLI contract smoke is missing an input")
endif()

set(created_output "${OUTPUT}.wave.json")
set(updated_output "${OUTPUT}-updated.wave.json")
set(invalid_output "${OUTPUT}-invalid.wave.json")
file(REMOVE "${created_output}" "${updated_output}" "${invalid_output}")

execute_process(
    COMMAND "${WAVE_CLI}" new "${OUTPUT}"
            "--name=Symbolic stimulus"
            "--duration=40 ns"
            "--operations=${NEW_OPERATIONS}"
            --dry-run
    RESULT_VARIABLE new_dry_code
    OUTPUT_VARIABLE new_dry_json
    ERROR_VARIABLE new_dry_error
)
if(NOT new_dry_code EQUAL 0 OR EXISTS "${created_output}")
    message(FATAL_ERROR "symbolic new dry-run failed: ${new_dry_error}")
endif()
string(JSON new_dry_sha GET "${new_dry_json}" resultSha256)
string(JSON new_operation_count GET "${new_dry_json}" operationCount)
string(JSON group_id GET "${new_dry_json}" operations 0 laneId)
string(JSON clock_group_id GET "${new_dry_json}" operations 1 groupId)
string(JSON bit_group_id GET "${new_dry_json}" operations 2 groupId)
string(JSON enum_group_id GET "${new_dry_json}" operations 3 groupId)
string(JSON enum_symbol_count GET "${new_dry_json}" operations 3 enumSymbolCount)
if(NOT new_operation_count EQUAL 6
   OR group_id STREQUAL "Control"
   OR NOT clock_group_id STREQUAL "${group_id}"
   OR NOT bit_group_id STREQUAL "${group_id}"
   OR NOT enum_group_id STREQUAL "${group_id}"
   OR NOT enum_symbol_count EQUAL 3)
    message(FATAL_ERROR "symbolic new dry-run did not canonicalize Group/Enum metadata")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" new "${OUTPUT}"
            "--name=Symbolic stimulus"
            "--duration=40 ns"
            "--operations=${NEW_OPERATIONS}"
    RESULT_VARIABLE new_code
    OUTPUT_VARIABLE new_json
    ERROR_VARIABLE new_error
)
if(NOT new_code EQUAL 0 OR NOT EXISTS "${created_output}")
    message(FATAL_ERROR "symbolic new write failed: ${new_error}")
endif()
string(JSON new_sha GET "${new_json}" resultSha256)
if(NOT new_sha STREQUAL "${new_dry_sha}")
    message(FATAL_ERROR "symbolic new dry-run and write SHA differ")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" signals "${created_output}"
            "--match=STATE" --kind=enum --exact
    RESULT_VARIABLE signals_code
    OUTPUT_VARIABLE signals_json
    ERROR_VARIABLE signals_error
)
if(NOT signals_code EQUAL 0)
    message(FATAL_ERROR "symbolic signal lookup failed: ${signals_error}")
endif()
string(JSON signal_matches GET "${signals_json}" matchCount)
string(JSON signal_group_name GET "${signals_json}" signals 0 groupName)
string(JSON signal_clock_name GET "${signals_json}" signals 0 clockDomainName)
string(JSON signal_done_value GET "${signals_json}" signals 0 enumMap DONE)
string(JSON signal_segments
       ERROR_VARIABLE signal_segments_error
       GET "${signals_json}" signals 0 segments)
if(NOT signal_matches EQUAL 1
   OR NOT signal_group_name STREQUAL "Control"
   OR NOT signal_clock_name STREQUAL "clk"
   OR NOT signal_done_value STREQUAL "2"
   OR NOT signal_segments_error)
    message(FATAL_ERROR "symbolic signal lookup returned incomplete or non-compact metadata")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" sample "${created_output}"
            "--lane=state" "--clock=CLK" "--at=cycle 2"
    RESULT_VARIABLE done_code
    OUTPUT_VARIABLE done_json
    ERROR_VARIABLE done_error
)
if(NOT done_code EQUAL 0)
    message(FATAL_ERROR "symbolic DONE sample failed: ${done_error}")
endif()
string(JSON done_value GET "${done_json}" samples 0 value)
if(NOT done_value STREQUAL "DONE")
    message(FATAL_ERROR "symbolic sequence did not preserve Enum values")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" apply "${created_output}" "${UPDATE_OPERATIONS}"
            "--output=${updated_output}" --dry-run
    RESULT_VARIABLE update_dry_code
    OUTPUT_VARIABLE update_dry_json
    ERROR_VARIABLE update_dry_error
)
if(NOT update_dry_code EQUAL 0 OR EXISTS "${updated_output}")
    message(FATAL_ERROR "symbolic update dry-run failed: ${update_dry_error}")
endif()
string(JSON update_dry_sha GET "${update_dry_json}" resultSha256)
string(JSON update_symbol_count GET "${update_dry_json}" operations 1 enumSymbolCount)
string(JSON update_error_value GET "${update_dry_json}" operations 1 enumMap ERROR)
if(NOT update_symbol_count EQUAL 4
   OR NOT update_error_value STREQUAL "3")
    message(FATAL_ERROR "Enum map replacement was not reported")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" apply "${created_output}" "${UPDATE_OPERATIONS}"
            "--output=${updated_output}"
    RESULT_VARIABLE update_code
    OUTPUT_VARIABLE update_json
    ERROR_VARIABLE update_error
)
if(NOT update_code EQUAL 0 OR NOT EXISTS "${updated_output}")
    message(FATAL_ERROR "symbolic update write failed: ${update_error}")
endif()
string(JSON update_sha GET "${update_json}" resultSha256)
if(NOT update_sha STREQUAL "${update_dry_sha}")
    message(FATAL_ERROR "symbolic update dry-run and write SHA differ")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" sample "${updated_output}"
            "--lane=state" "--clock=clk" "--at=cycle 3"
    RESULT_VARIABLE error_sample_code
    OUTPUT_VARIABLE error_sample_json
    ERROR_VARIABLE error_sample_error
)
if(NOT error_sample_code EQUAL 0)
    message(FATAL_ERROR "symbolic ERROR sample failed: ${error_sample_error}")
endif()
string(JSON error_sample_value GET "${error_sample_json}" samples 0 value)
if(NOT error_sample_value STREQUAL "ERROR")
    message(FATAL_ERROR "updated Enum value was not observable")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" validate "${updated_output}"
    RESULT_VARIABLE validate_code
    OUTPUT_VARIABLE validate_json
    ERROR_VARIABLE validate_error
)
if(NOT validate_code EQUAL 0)
    message(FATAL_ERROR "updated symbolic project did not validate: ${validate_error}")
endif()
string(JSON validation_errors GET "${validate_json}" summary errors)
if(NOT validation_errors STREQUAL "0")
    message(FATAL_ERROR "updated symbolic project contains validation errors")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" apply "${created_output}" "${INVALID_OPERATIONS}"
            "--output=${invalid_output}"
    RESULT_VARIABLE invalid_code
    OUTPUT_VARIABLE invalid_output_json
    ERROR_VARIABLE invalid_json
)
if(NOT invalid_code EQUAL 4 OR EXISTS "${invalid_output}")
    message(FATAL_ERROR "overflowing Enum map was accepted or wrote an output")
endif()
string(JSON invalid_error_code GET "${invalid_json}" error code)
string(JSON invalid_operation_index GET "${invalid_json}" error operation)
if(NOT invalid_error_code STREQUAL "operation-rejected"
   OR NOT invalid_operation_index EQUAL 0)
    message(FATAL_ERROR "invalid Enum map returned the wrong failure contract")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" signals "${created_output}"
            "--match=state" --kind=enum --exact
    RESULT_VARIABLE source_code
    OUTPUT_VARIABLE source_json
    ERROR_VARIABLE source_error
)
if(NOT source_code EQUAL 0)
    message(FATAL_ERROR "source re-query after rejected update failed: ${source_error}")
endif()
string(JSON source_sha GET "${source_json}" sourceSha256)
if(NOT source_sha STREQUAL "${new_sha}")
    message(FATAL_ERROR "rejected Enum update changed the source project")
endif()

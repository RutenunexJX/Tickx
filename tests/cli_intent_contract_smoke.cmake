if(NOT DEFINED WAVE_CLI
   OR NOT DEFINED NEW_OPERATIONS
   OR NOT DEFINED INTENT_OPERATIONS
   OR NOT DEFINED CLEANUP_OPERATIONS
   OR NOT DEFINED INVALID_OPERATIONS
   OR NOT DEFINED OUTPUT)
    message(FATAL_ERROR "Intent CLI contract smoke is missing an input")
endif()

set(source_output "${OUTPUT}.wave.json")
set(intent_output "${OUTPUT}-intent.wave.json")
set(cleanup_output "${OUTPUT}-cleanup.wave.json")
set(invalid_output "${OUTPUT}-invalid.wave.json")
file(REMOVE
    "${source_output}"
    "${intent_output}"
    "${cleanup_output}"
    "${invalid_output}")

execute_process(
    COMMAND "${WAVE_CLI}" new "${OUTPUT}"
            "--name=Intent stimulus"
            "--duration=40 ns"
            "--operations=${NEW_OPERATIONS}"
    RESULT_VARIABLE new_code
    OUTPUT_VARIABLE new_json
    ERROR_VARIABLE new_error
)
if(NOT new_code EQUAL 0 OR NOT EXISTS "${source_output}")
    message(FATAL_ERROR "intent source creation failed: ${new_error}")
endif()
string(JSON source_sha GET "${new_json}" resultSha256)

execute_process(
    COMMAND "${WAVE_CLI}" apply "${source_output}" "${INTENT_OPERATIONS}"
            "--output=${intent_output}" --dry-run
    RESULT_VARIABLE intent_dry_code
    OUTPUT_VARIABLE intent_dry_json
    ERROR_VARIABLE intent_dry_error
)
if(NOT intent_dry_code EQUAL 0 OR EXISTS "${intent_output}")
    message(FATAL_ERROR "intent dry-run failed: ${intent_dry_error}")
endif()
string(JSON intent_dry_sha GET "${intent_dry_json}" resultSha256)
string(JSON intent_operation_count GET "${intent_dry_json}" operationCount)
string(JSON relation_id GET "${intent_dry_json}" operations 0 relationId)
string(JSON relation_source_tick GET "${intent_dry_json}" operations 2 sourceAtTick)
string(JSON relation_minimum GET "${intent_dry_json}" operations 2 minimumDelayTick)
string(JSON relation_maximum GET "${intent_dry_json}" operations 2 maximumDelayTick)
string(JSON marker_id GET "${intent_dry_json}" operations 1 markerId)
string(JSON marker_name GET "${intent_dry_json}" operations 3 name)
string(JSON marker_start GET "${intent_dry_json}" operations 3 startTick)
string(JSON marker_end GET "${intent_dry_json}" operations 3 endTick)
string(JSON relation_no_effect GET "${intent_dry_json}" operations 4 changed)
string(JSON marker_no_effect GET "${intent_dry_json}" operations 5 changed)
string(JSON relation_count
       GET "${intent_dry_json}" changes scenarios 0 afterRelationCount)
string(JSON marker_count
       GET "${intent_dry_json}" changes scenarios 0 afterMarkerCount)
string(JSON validation_errors
       GET "${intent_dry_json}" validation summary errors)
string(JSON validation_information
       GET "${intent_dry_json}" validation summary information)
if(NOT intent_operation_count EQUAL 6
   OR NOT relation_id STREQUAL "relation-enable-state"
   OR NOT relation_source_tick STREQUAL "10000"
   OR NOT relation_minimum STREQUAL "10000"
   OR NOT relation_maximum STREQUAL "20000"
   OR marker_id STREQUAL "Active"
   OR NOT marker_name STREQUAL "Control window"
   OR NOT marker_start STREQUAL "10000"
   OR NOT marker_end STREQUAL "40000"
   OR relation_no_effect
   OR marker_no_effect
   OR NOT relation_count EQUAL 1
   OR NOT marker_count EQUAL 1
   OR NOT validation_errors STREQUAL "0"
   OR NOT validation_information STREQUAL "1")
    message(FATAL_ERROR "intent dry-run returned the wrong Relation/Marker contract")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" apply "${source_output}" "${INTENT_OPERATIONS}"
            "--output=${intent_output}"
    RESULT_VARIABLE intent_code
    OUTPUT_VARIABLE intent_json
    ERROR_VARIABLE intent_error
)
if(NOT intent_code EQUAL 0 OR NOT EXISTS "${intent_output}")
    message(FATAL_ERROR "intent write failed: ${intent_error}")
endif()
string(JSON intent_sha GET "${intent_json}" resultSha256)
if(NOT intent_sha STREQUAL "${intent_dry_sha}")
    message(FATAL_ERROR "intent dry-run and write SHA differ")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" window "${intent_output}"
            "--start=cycle 0" "--end=cycle 4"
            "--clock=clk" "--lane=enable" "--lane=state"
    RESULT_VARIABLE window_code
    OUTPUT_VARIABLE window_json
    ERROR_VARIABLE window_error
)
if(NOT window_code EQUAL 0)
    message(FATAL_ERROR "intent window query failed: ${window_error}")
endif()
string(JSON window_relation_count GET "${window_json}" relationCount)
string(JSON window_marker_count GET "${window_json}" markerCount)
string(JSON window_relation_id GET "${window_json}" relations 0 id)
string(JSON window_marker_name GET "${window_json}" markers 0 name)
if(NOT window_relation_count EQUAL 1
   OR NOT window_marker_count EQUAL 1
   OR NOT window_relation_id STREQUAL "relation-enable-state"
   OR NOT window_marker_name STREQUAL "Control window")
    message(FATAL_ERROR "intent objects were not visible in the local window")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" apply "${intent_output}" "${CLEANUP_OPERATIONS}"
            "--output=${cleanup_output}"
    RESULT_VARIABLE cleanup_code
    OUTPUT_VARIABLE cleanup_json
    ERROR_VARIABLE cleanup_error
)
if(NOT cleanup_code EQUAL 0 OR NOT EXISTS "${cleanup_output}")
    message(FATAL_ERROR "intent cleanup failed: ${cleanup_error}")
endif()
string(JSON cleanup_relation_id
       GET "${cleanup_json}" operations 0 relationId)
string(JSON cleanup_marker_id
       GET "${cleanup_json}" operations 1 markerId)
string(JSON cleanup_relation_count
       GET "${cleanup_json}" changes scenarios 0 afterRelationCount)
string(JSON cleanup_marker_count
       GET "${cleanup_json}" changes scenarios 0 afterMarkerCount)
if(NOT cleanup_relation_id STREQUAL "${relation_id}"
   OR NOT cleanup_marker_id STREQUAL "${marker_id}"
   OR NOT cleanup_relation_count EQUAL 0
   OR NOT cleanup_marker_count EQUAL 0)
    message(FATAL_ERROR "intent cleanup did not return canonical identities")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" apply "${source_output}" "${INVALID_OPERATIONS}"
            "--output=${invalid_output}"
    RESULT_VARIABLE invalid_code
    OUTPUT_VARIABLE invalid_output_json
    ERROR_VARIABLE invalid_json
)
if(NOT invalid_code EQUAL 4 OR EXISTS "${invalid_output}")
    message(FATAL_ERROR "missing-edge Relation was accepted or wrote an output")
endif()
string(JSON invalid_error_code GET "${invalid_json}" error code)
string(JSON invalid_operation GET "${invalid_json}" error operation)
if(NOT invalid_error_code STREQUAL "operation-rejected"
   OR NOT invalid_operation EQUAL 1)
    message(FATAL_ERROR "missing-edge Relation returned the wrong failure contract")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" signals "${source_output}"
            "--match=state" --kind=enum --exact
    RESULT_VARIABLE source_code
    OUTPUT_VARIABLE source_json
    ERROR_VARIABLE source_error
)
if(NOT source_code EQUAL 0)
    message(FATAL_ERROR "source re-query after rejected intent failed: ${source_error}")
endif()
string(JSON source_after_sha GET "${source_json}" sourceSha256)
if(NOT source_after_sha STREQUAL "${source_sha}")
    message(FATAL_ERROR "rejected intent batch changed the source project")
endif()

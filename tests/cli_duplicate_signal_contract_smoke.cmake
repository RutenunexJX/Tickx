if(NOT DEFINED WAVE_CLI
   OR NOT DEFINED PROJECT
   OR NOT DEFINED OPERATIONS
   OR NOT DEFINED INVALID_OPERATIONS
   OR NOT DEFINED OUTPUT)
    message(FATAL_ERROR "Duplicate-signal CLI contract smoke is missing an input")
endif()

set(result_output "${OUTPUT}.wave.json")
set(invalid_output "${OUTPUT}-invalid.wave.json")
file(REMOVE "${result_output}" "${invalid_output}")

execute_process(
    COMMAND "${WAVE_CLI}" capabilities
    RESULT_VARIABLE capabilities_code
    OUTPUT_VARIABLE capabilities_json
    ERROR_VARIABLE capabilities_error
)
if(NOT capabilities_code EQUAL 0)
    message(FATAL_ERROR "duplicate capability discovery failed: ${capabilities_error}")
endif()
string(JSON operation_count GET "${capabilities_json}" operationCount)
string(JSON operation_length LENGTH "${capabilities_json}" operations)
set(has_duplicate_signal FALSE)
math(EXPR operation_last "${operation_length} - 1")
foreach(operation_index RANGE 0 ${operation_last})
    string(JSON operation_name
           GET "${capabilities_json}" operations ${operation_index} name)
    if(operation_name STREQUAL "duplicate-signal")
        set(has_duplicate_signal TRUE)
    endif()
endforeach()
if(NOT operation_count EQUAL 33
   OR NOT operation_length EQUAL 33
   OR NOT has_duplicate_signal)
    message(FATAL_ERROR "duplicate-signal is missing from capabilities")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" inspect "${PROJECT}" --summary
    RESULT_VARIABLE source_code
    OUTPUT_VARIABLE source_json
    ERROR_VARIABLE source_error
)
if(NOT source_code EQUAL 0)
    message(FATAL_ERROR "duplicate source inspection failed: ${source_error}")
endif()
string(JSON source_sha GET "${source_json}" sourceSha256)
string(JSON source_lane_count
       GET "${source_json}" project scenarios 0 laneCount)
string(JSON source_event_count
       GET "${source_json}" project scenarios 0 eventCount)
string(JSON source_relation_count
       GET "${source_json}" project scenarios 0 relationCount)
string(JSON source_marker_count
       GET "${source_json}" project scenarios 0 markerCount)
string(JSON source_clock_count
       GET "${source_json}" project clockDomainCount)

execute_process(
    COMMAND "${WAVE_CLI}" apply "${PROJECT}" "${OPERATIONS}"
            "--output=${result_output}" --dry-run
    RESULT_VARIABLE dry_code
    OUTPUT_VARIABLE dry_json
    ERROR_VARIABLE dry_error
)
if(NOT dry_code EQUAL 0 OR EXISTS "${result_output}")
    message(FATAL_ERROR "duplicate-signal dry-run failed: ${dry_error}")
endif()
string(JSON dry_sha GET "${dry_json}" resultSha256)
string(JSON duplicate_count GET "${dry_json}" operationCount)
string(JSON first_source GET "${dry_json}" operations 0 sourceLaneId)
string(JSON first_id GET "${dry_json}" operations 0 laneId)
string(JSON first_name GET "${dry_json}" operations 0 name)
string(JSON first_kind GET "${dry_json}" operations 0 kind)
string(JSON first_segments GET "${dry_json}" operations 0 segmentCount)
string(JSON first_color GET "${dry_json}" operations 0 color)
string(JSON first_events GET "${dry_json}" operations 0 copiedEventCount)
string(JSON first_relations GET "${dry_json}" operations 0 copiedRelationCount)
string(JSON first_mappings GET "${dry_json}" operations 0 copiedTraceMappingCount)
string(JSON second_source GET "${dry_json}" operations 1 sourceLaneId)
string(JSON second_id GET "${dry_json}" operations 1 laneId)
string(JSON second_name GET "${dry_json}" operations 1 name)
string(JSON clock_source GET "${dry_json}" operations 2 sourceLaneId)
string(JSON clock_id GET "${dry_json}" operations 2 laneId)
string(JSON clock_domain GET "${dry_json}" operations 2 clockId)
string(JSON independent_clock
       GET "${dry_json}" operations 2 independentClockDomain)
string(JSON after_lane_count
       GET "${dry_json}" changes scenarios 0 afterLaneCount)
string(JSON after_event_count
       GET "${dry_json}" changes scenarios 0 afterEventCount)
string(JSON after_relation_count
       GET "${dry_json}" changes scenarios 0 afterRelationCount)
string(JSON after_marker_count
       GET "${dry_json}" changes scenarios 0 afterMarkerCount)
string(JSON after_clock_count GET "${dry_json}" changes afterClockCount)
math(EXPR expected_lane_count "${source_lane_count} + 3")
math(EXPR expected_clock_count "${source_clock_count} + 1")
if(NOT duplicate_count EQUAL 3
   OR NOT first_source STREQUAL "lane-data"
   OR first_id STREQUAL "lane-data"
   OR NOT first_name STREQUAL "payload_copy"
   OR NOT first_kind STREQUAL "bus"
   OR NOT first_segments EQUAL 3
   OR first_color STREQUAL "#ce93d8"
   OR NOT first_events EQUAL 0
   OR NOT first_relations EQUAL 0
   OR NOT first_mappings EQUAL 0
   OR NOT second_source STREQUAL "${first_id}"
   OR second_id STREQUAL "${first_id}"
   OR NOT second_name STREQUAL "payload_copy_copy"
   OR NOT clock_source STREQUAL "lane-clk"
   OR clock_id STREQUAL "lane-clk"
   OR NOT clock_domain STREQUAL "clock-shadow"
   OR NOT independent_clock
   OR NOT after_lane_count EQUAL expected_lane_count
   OR NOT after_clock_count EQUAL expected_clock_count
   OR NOT after_event_count EQUAL source_event_count
   OR NOT after_relation_count EQUAL source_relation_count
   OR NOT after_marker_count EQUAL source_marker_count)
    message(FATAL_ERROR "duplicate-signal dry-run returned the wrong copy contract")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" apply "${PROJECT}" "${OPERATIONS}"
            "--output=${result_output}"
    RESULT_VARIABLE write_code
    OUTPUT_VARIABLE write_json
    ERROR_VARIABLE write_error
)
if(NOT write_code EQUAL 0 OR NOT EXISTS "${result_output}")
    message(FATAL_ERROR "duplicate-signal write failed: ${write_error}")
endif()
string(JSON write_sha GET "${write_json}" resultSha256)
if(NOT write_sha STREQUAL "${dry_sha}")
    message(FATAL_ERROR "duplicate-signal dry-run and write SHA differ")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" sample "${result_output}" "--at=90 ns"
            "--lane=data[7:0]" --lane=payload_copy
            --lane=payload_copy_copy
    RESULT_VARIABLE bus_sample_code
    OUTPUT_VARIABLE bus_sample_json
    ERROR_VARIABLE bus_sample_error
)
if(NOT bus_sample_code EQUAL 0)
    message(FATAL_ERROR "duplicated Bus sampling failed: ${bus_sample_error}")
endif()
string(JSON original_bus_value GET "${bus_sample_json}" samples 0 value)
string(JSON first_bus_value GET "${bus_sample_json}" samples 1 value)
string(JSON second_bus_value GET "${bus_sample_json}" samples 2 value)
if(NOT original_bus_value STREQUAL "${first_bus_value}"
   OR NOT first_bus_value STREQUAL "${second_bus_value}")
    message(FATAL_ERROR "duplicated Bus waveform differs from its source")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" sample "${result_output}" "--at=175 ns"
            --lane=clk --lane=clk_shadow
    RESULT_VARIABLE clock_sample_code
    OUTPUT_VARIABLE clock_sample_json
    ERROR_VARIABLE clock_sample_error
)
if(NOT clock_sample_code EQUAL 0)
    message(FATAL_ERROR "duplicated Clock sampling failed: ${clock_sample_error}")
endif()
string(JSON original_clock_value GET "${clock_sample_json}" samples 0 value)
string(JSON duplicate_clock_value GET "${clock_sample_json}" samples 1 value)
if(NOT original_clock_value STREQUAL "${duplicate_clock_value}")
    message(FATAL_ERROR "duplicated Clock waveform differs from its source")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" inspect "${result_output}" --summary
    RESULT_VARIABLE inspect_code
    OUTPUT_VARIABLE inspect_json
    ERROR_VARIABLE inspect_error
)
if(NOT inspect_code EQUAL 0)
    message(FATAL_ERROR "duplicated project inspection failed: ${inspect_error}")
endif()
string(JSON result_clock_count
       GET "${inspect_json}" project clockDomainCount)
string(JSON shadow_clock_id
       GET "${inspect_json}" project clockDomains 1 id)
string(JSON shadow_clock_name
       GET "${inspect_json}" project clockDomains 1 name)
string(JSON shadow_clock_period
       GET "${inspect_json}" project clockDomains 1 periodTick)
string(JSON source_clock_period
       GET "${inspect_json}" project clockDomains 0 periodTick)
if(NOT result_clock_count EQUAL expected_clock_count
   OR NOT shadow_clock_id STREQUAL "clock-shadow"
   OR NOT shadow_clock_name STREQUAL "clk_shadow"
   OR NOT shadow_clock_period STREQUAL "${source_clock_period}")
    message(FATAL_ERROR "duplicated ClockDomain is not independent and equivalent")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" validate "${result_output}"
    RESULT_VARIABLE validate_code
    OUTPUT_VARIABLE validate_json
    ERROR_VARIABLE validate_error
)
if(NOT validate_code EQUAL 0)
    message(FATAL_ERROR "duplicated project validation failed: ${validate_error}")
endif()
string(JSON validation_errors GET "${validate_json}" summary errors)
if(NOT validation_errors STREQUAL "0")
    message(FATAL_ERROR "duplicated project introduced validation errors")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" apply "${PROJECT}" "${INVALID_OPERATIONS}"
            "--output=${invalid_output}"
    RESULT_VARIABLE invalid_code
    OUTPUT_VARIABLE invalid_stdout
    ERROR_VARIABLE invalid_json
)
if(NOT invalid_code EQUAL 4
   OR NOT invalid_stdout STREQUAL ""
   OR EXISTS "${invalid_output}")
    message(FATAL_ERROR "invalid duplicate-signal batch was accepted or wrote output")
endif()
string(JSON invalid_error_code GET "${invalid_json}" error code)
string(JSON invalid_operation GET "${invalid_json}" error operation)
if(NOT invalid_error_code STREQUAL "operation-rejected"
   OR NOT invalid_operation EQUAL 1)
    message(FATAL_ERROR "invalid duplicate-signal returned the wrong failure contract")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" inspect "${PROJECT}" --summary
    RESULT_VARIABLE source_after_code
    OUTPUT_VARIABLE source_after_json
    ERROR_VARIABLE source_after_error
)
if(NOT source_after_code EQUAL 0)
    message(FATAL_ERROR "duplicate source reinspection failed: ${source_after_error}")
endif()
string(JSON source_after_sha GET "${source_after_json}" sourceSha256)
if(NOT source_after_sha STREQUAL "${source_sha}")
    message(FATAL_ERROR "rejected duplicate-signal batch changed the source")
endif()

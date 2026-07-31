if(NOT DEFINED WAVE_CLI OR NOT DEFINED PROJECT)
    message(FATAL_ERROR "Relations CLI contract smoke is missing an input")
endif()

file(SHA256 "${PROJECT}" source_sha_before)

execute_process(
    COMMAND "${WAVE_CLI}" capabilities
    RESULT_VARIABLE capabilities_code
    OUTPUT_VARIABLE capabilities_json
    ERROR_VARIABLE capabilities_error
)
if(NOT capabilities_code EQUAL 0)
    message(FATAL_ERROR "capabilities failed: ${capabilities_error}")
endif()
string(JSON command_count GET "${capabilities_json}" commandCount)
string(JSON compact_relation_query
       GET "${capabilities_json}" features compactRelationQuery)
set(has_relations FALSE)
math(EXPR command_last "${command_count} - 1")
foreach(command_index RANGE 0 ${command_last})
    string(JSON command_name
           GET "${capabilities_json}" commands ${command_index} name)
    if(command_name STREQUAL "relations")
        set(has_relations TRUE)
    endif()
endforeach()
if(NOT command_count EQUAL 11
   OR NOT compact_relation_query
   OR NOT has_relations)
    message(FATAL_ERROR "capabilities does not advertise the relations contract")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" relations "${PROJECT}"
            "--match=WITHIN" --severity=error
            "--start=90 ns" "--end=120 ns"
            --scenario=scenario-handshake --lane=ACK
    RESULT_VARIABLE relation_code
    OUTPUT_VARIABLE relation_json
    ERROR_VARIABLE relation_error
)
if(NOT relation_code EQUAL 0)
    message(FATAL_ERROR "Relation query failed: ${relation_error}")
endif()
string(JSON relation_command GET "${relation_json}" command)
string(JSON range_filtered GET "${relation_json}" rangeFiltered)
string(JSON start_tick GET "${relation_json}" startTick)
string(JSON end_tick GET "${relation_json}" endTick)
string(JSON lane_filter_count GET "${relation_json}" laneFilterCount)
string(JSON scenario_count GET "${relation_json}" scenarioRelationCount)
string(JSON match_count GET "${relation_json}" matchCount)
string(JSON ready_count GET "${relation_json}" readyCount)
string(JSON issue_count GET "${relation_json}" endpointIssueCount)
string(JSON relation_id GET "${relation_json}" relations 0 relationId)
string(JSON endpoints_ready GET "${relation_json}" relations 0 endpointsReady)
string(JSON observed_delay GET "${relation_json}" relations 0 observedDelayTick)
string(JSON timing_ok GET "${relation_json}" relations 0 timingWithinRange)
string(JSON source_lane GET "${relation_json}" relations 0 source laneId)
string(JSON source_name GET "${relation_json}" relations 0 source name)
string(JSON source_tick GET "${relation_json}" relations 0 source timeTick)
string(JSON source_edge GET "${relation_json}" relations 0 source edge)
string(JSON source_value GET "${relation_json}" relations 0 source value)
string(JSON source_endpoint
       GET "${relation_json}" relations 0 source relationEndpoint)
string(JSON target_lane GET "${relation_json}" relations 0 target laneId)
string(JSON target_name GET "${relation_json}" relations 0 target name)
string(JSON target_tick GET "${relation_json}" relations 0 target timeTick)
string(JSON relation_row GET "${relation_json}" relations 0)
string(FIND "${relation_row}" "\"sourceEventId\"" source_event_position)
string(FIND "${relation_row}" "\"targetEventId\"" target_event_position)
string(FIND "${relation_row}" "\"linkedSegmentId\"" segment_position)
if(NOT relation_command STREQUAL "relations"
   OR NOT range_filtered
   OR NOT start_tick STREQUAL "90000"
   OR NOT end_tick STREQUAL "120000"
   OR NOT lane_filter_count EQUAL 1
   OR NOT scenario_count EQUAL 1
   OR NOT match_count EQUAL 1
   OR NOT ready_count EQUAL 1
   OR NOT issue_count EQUAL 0
   OR NOT relation_id STREQUAL "relation-req-ack"
   OR NOT endpoints_ready
   OR NOT observed_delay STREQUAL "30000"
   OR NOT timing_ok
   OR NOT source_lane STREQUAL "lane-request"
   OR NOT source_name STREQUAL "req"
   OR NOT source_tick STREQUAL "80000"
   OR NOT source_edge STREQUAL "rising"
   OR NOT source_value STREQUAL "1"
   OR NOT source_endpoint
   OR NOT target_lane STREQUAL "lane-ack"
   OR NOT target_name STREQUAL "ack"
   OR NOT target_tick STREQUAL "110000"
   OR NOT source_event_position EQUAL -1
   OR NOT target_event_position EQUAL -1
   OR NOT segment_position EQUAL -1)
    message(FATAL_ERROR "Relation query returned the wrong compact result")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" relations "${PROJECT}"
            --match=RELATION-REQ-ACK --exact --lane=req
    RESULT_VARIABLE exact_code
    OUTPUT_VARIABLE exact_json
    ERROR_VARIABLE exact_error
)
if(NOT exact_code EQUAL 0)
    message(FATAL_ERROR "exact Relation query failed: ${exact_error}")
endif()
string(JSON exact_match_count GET "${exact_json}" matchCount)
string(JSON exact_relation_id GET "${exact_json}" relations 0 relationId)
if(NOT exact_match_count EQUAL 1
   OR NOT exact_relation_id STREQUAL "relation-req-ack")
    message(FATAL_ERROR "exact Relation query did not match the stable ID")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" relations "${PROJECT}"
            "--start=cycle 9" "--end=cycle 12" --lane=ack
    RESULT_VARIABLE cycle_code
    OUTPUT_VARIABLE cycle_json
    ERROR_VARIABLE cycle_error
)
if(NOT cycle_code EQUAL 0)
    message(FATAL_ERROR "cycle Relation query failed: ${cycle_error}")
endif()
string(JSON cycle_start GET "${cycle_json}" startTick)
string(JSON cycle_end GET "${cycle_json}" endTick)
string(JSON cycle_clock GET "${cycle_json}" clockId)
string(JSON cycle_match_count GET "${cycle_json}" matchCount)
if(NOT cycle_start STREQUAL "90000"
   OR NOT cycle_end STREQUAL "120000"
   OR NOT cycle_clock STREQUAL "clock-main"
   OR NOT cycle_match_count EQUAL 1)
    message(FATAL_ERROR "cycle Relation query used the wrong range")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" relations "${PROJECT}" --lane=lane-data
    RESULT_VARIABLE empty_code
    OUTPUT_VARIABLE empty_json
    ERROR_VARIABLE empty_error
)
if(NOT empty_code EQUAL 0)
    message(FATAL_ERROR "empty Relation query failed: ${empty_error}")
endif()
string(JSON empty_match_count GET "${empty_json}" matchCount)
string(JSON empty_returned_count GET "${empty_json}" returnedCount)
if(NOT empty_match_count EQUAL 0 OR NOT empty_returned_count EQUAL 0)
    message(FATAL_ERROR "unrelated Lane returned a Relation")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" relations "${PROJECT}" --severity=fatal
    RESULT_VARIABLE severity_code
    OUTPUT_VARIABLE severity_output
    ERROR_VARIABLE severity_json
)
if(NOT severity_code EQUAL 2 OR NOT severity_output STREQUAL "")
    message(FATAL_ERROR "invalid Relation severity was accepted")
endif()
string(JSON severity_error GET "${severity_json}" error code)
if(NOT severity_error STREQUAL "usage")
    message(FATAL_ERROR "invalid Relation severity was not structured")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" relations "${PROJECT}" --exact
    RESULT_VARIABLE exact_usage_code
    OUTPUT_VARIABLE exact_usage_output
    ERROR_VARIABLE exact_usage_json
)
if(NOT exact_usage_code EQUAL 2 OR NOT exact_usage_output STREQUAL "")
    message(FATAL_ERROR "Relation --exact was accepted without --match")
endif()
string(JSON exact_usage_error GET "${exact_usage_json}" error code)
if(NOT exact_usage_error STREQUAL "usage")
    message(FATAL_ERROR "Relation --exact failure was not structured")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" relations "${PROJECT}"
            "--start=130 ns" "--end=70 ns"
    RESULT_VARIABLE range_code
    OUTPUT_VARIABLE range_output
    ERROR_VARIABLE range_json
)
if(NOT range_code EQUAL 4 OR NOT range_output STREQUAL "")
    message(FATAL_ERROR "invalid Relation range was accepted")
endif()
string(JSON range_error GET "${range_json}" error code)
if(NOT range_error STREQUAL "relation-query-rejected")
    message(FATAL_ERROR "invalid Relation range was not structured")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" relations "${PROJECT}"
            --lane=req --lane=lane-request
    RESULT_VARIABLE duplicate_code
    OUTPUT_VARIABLE duplicate_output
    ERROR_VARIABLE duplicate_json
)
if(NOT duplicate_code EQUAL 4 OR NOT duplicate_output STREQUAL "")
    message(FATAL_ERROR "duplicate Relation Lane filters were accepted")
endif()
string(JSON duplicate_error GET "${duplicate_json}" error code)
if(NOT duplicate_error STREQUAL "selector-invalid")
    message(FATAL_ERROR "duplicate Relation Lane filters were not structured")
endif()

file(SHA256 "${PROJECT}" source_sha_after)
if(NOT source_sha_after STREQUAL source_sha_before)
    message(FATAL_ERROR "read-only Relation queries changed the source project")
endif()

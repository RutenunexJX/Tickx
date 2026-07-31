if(NOT DEFINED WAVE_CLI OR NOT DEFINED PROJECT)
    message(FATAL_ERROR "Edges CLI contract smoke is missing an input")
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
string(JSON edge_kind_count LENGTH "${capabilities_json}" values edgeKinds)
string(JSON relation_ready
       GET "${capabilities_json}" features relationReadyEdgeQuery)
set(has_edges FALSE)
math(EXPR command_last "${command_count} - 1")
foreach(command_index RANGE 0 ${command_last})
    string(JSON command_name
           GET "${capabilities_json}" commands ${command_index} name)
    if(command_name STREQUAL "edges")
        set(has_edges TRUE)
    endif()
endforeach()
if(NOT command_count EQUAL 11
   OR NOT edge_kind_count EQUAL 4
   OR NOT relation_ready
   OR NOT has_edges)
    message(FATAL_ERROR "capabilities does not advertise the edges contract")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" edges "${PROJECT}"
            "--start=70 ns" "--end=130 ns" --edge=rising
            --scenario=scenario-handshake --lane=req --lane=ack
    RESULT_VARIABLE rising_code
    OUTPUT_VARIABLE rising_json
    ERROR_VARIABLE rising_error
)
if(NOT rising_code EQUAL 0)
    message(FATAL_ERROR "rising edge query failed: ${rising_error}")
endif()
string(JSON rising_command GET "${rising_json}" command)
string(JSON rising_start GET "${rising_json}" startTick)
string(JSON rising_end GET "${rising_json}" endTick)
string(JSON rising_lanes GET "${rising_json}" laneCount)
string(JSON rising_matches GET "${rising_json}" matchCount)
string(JSON rising_returned GET "${rising_json}" returnedCount)
string(JSON rising_ambiguous GET "${rising_json}" ambiguousEndpointCount)
string(JSON first_lane GET "${rising_json}" edges 0 laneId)
string(JSON first_tick GET "${rising_json}" edges 0 timeTick)
string(JSON first_edge GET "${rising_json}" edges 0 edge)
string(JSON first_previous GET "${rising_json}" edges 0 previousValue)
string(JSON first_value GET "${rising_json}" edges 0 value)
string(JSON first_endpoint GET "${rising_json}" edges 0 relationEndpoint)
string(JSON second_lane GET "${rising_json}" edges 1 laneId)
string(JSON second_tick GET "${rising_json}" edges 1 timeTick)
string(JSON first_row GET "${rising_json}" edges 0)
string(FIND "${first_row}" "\"eventId\"" event_id_position)
string(FIND "${first_row}" "\"segmentId\"" segment_id_position)
if(NOT rising_command STREQUAL "edges"
   OR NOT rising_start STREQUAL "70000"
   OR NOT rising_end STREQUAL "130000"
   OR NOT rising_lanes EQUAL 2
   OR NOT rising_matches EQUAL 2
   OR NOT rising_returned EQUAL 2
   OR NOT rising_ambiguous EQUAL 0
   OR NOT first_lane STREQUAL "lane-request"
   OR NOT first_tick STREQUAL "80000"
   OR NOT first_edge STREQUAL "rising"
   OR NOT first_previous STREQUAL "0"
   OR NOT first_value STREQUAL "1"
   OR NOT first_endpoint
   OR NOT second_lane STREQUAL "lane-ack"
   OR NOT second_tick STREQUAL "110000"
   OR NOT event_id_position EQUAL -1
   OR NOT segment_id_position EQUAL -1)
    message(FATAL_ERROR "rising edge query returned the wrong compact result")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" edges "${PROJECT}"
            --edge=falling --limit=1 --lane=req --lane=ack
    RESULT_VARIABLE falling_code
    OUTPUT_VARIABLE falling_json
    ERROR_VARIABLE falling_error
)
if(NOT falling_code EQUAL 0)
    message(FATAL_ERROR "falling edge query failed: ${falling_error}")
endif()
string(JSON falling_matches GET "${falling_json}" matchCount)
string(JSON falling_returned GET "${falling_json}" returnedCount)
string(JSON falling_truncated GET "${falling_json}" truncated)
string(JSON falling_tick GET "${falling_json}" edges 0 timeTick)
if(NOT falling_matches EQUAL 2
   OR NOT falling_returned EQUAL 1
   OR NOT falling_truncated
   OR NOT falling_tick STREQUAL "130000")
    message(FATAL_ERROR "falling edge limit did not report truncation")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" edges "${PROJECT}"
            --edge=change "--lane=data[7:0]"
    RESULT_VARIABLE data_code
    OUTPUT_VARIABLE data_json
    ERROR_VARIABLE data_error
)
if(NOT data_code EQUAL 0)
    message(FATAL_ERROR "Bus edge query failed: ${data_error}")
endif()
string(JSON data_matches GET "${data_json}" matchCount)
string(JSON data_first_tick GET "${data_json}" edges 0 timeTick)
string(JSON data_second_tick GET "${data_json}" edges 1 timeTick)
if(NOT data_matches EQUAL 2
   OR NOT data_first_tick STREQUAL "80000"
   OR NOT data_second_tick STREQUAL "150000")
    message(FATAL_ERROR "Bus change query returned the wrong transitions")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" edges "${PROJECT}"
            "--start=cycle 8" "--end=cycle 13" --edge=rising
            --lane=req --lane=ack
    RESULT_VARIABLE cycle_code
    OUTPUT_VARIABLE cycle_json
    ERROR_VARIABLE cycle_error
)
if(NOT cycle_code EQUAL 0)
    message(FATAL_ERROR "cycle edge query failed: ${cycle_error}")
endif()
string(JSON cycle_start GET "${cycle_json}" startTick)
string(JSON cycle_end GET "${cycle_json}" endTick)
string(JSON cycle_clock GET "${cycle_json}" clockId)
string(JSON cycle_matches GET "${cycle_json}" matchCount)
if(NOT cycle_start STREQUAL "80000"
   OR NOT cycle_end STREQUAL "130000"
   OR NOT cycle_clock STREQUAL "clock-main"
   OR NOT cycle_matches EQUAL 2)
    message(FATAL_ERROR "cycle edge query used the wrong clock range")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" edges "${PROJECT}" --lane=clk
    RESULT_VARIABLE clock_code
    OUTPUT_VARIABLE clock_output
    ERROR_VARIABLE clock_json
)
if(NOT clock_code EQUAL 4 OR NOT clock_output STREQUAL "")
    message(FATAL_ERROR "Clock Lane was accepted as a Relation endpoint")
endif()
string(JSON clock_error GET "${clock_json}" error code)
if(NOT clock_error STREQUAL "edge-query-rejected")
    message(FATAL_ERROR "Clock Lane rejection was not structured")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" edges "${PROJECT}" --edge=sideways
    RESULT_VARIABLE invalid_edge_code
    OUTPUT_VARIABLE invalid_edge_output
    ERROR_VARIABLE invalid_edge_json
)
if(NOT invalid_edge_code EQUAL 2 OR NOT invalid_edge_output STREQUAL "")
    message(FATAL_ERROR "invalid edge filter was accepted")
endif()
string(JSON invalid_edge_error GET "${invalid_edge_json}" error code)
if(NOT invalid_edge_error STREQUAL "usage")
    message(FATAL_ERROR "invalid edge filter was not a structured usage error")
endif()

file(SHA256 "${PROJECT}" source_sha_after)
if(NOT source_sha_after STREQUAL source_sha_before)
    message(FATAL_ERROR "read-only edge queries changed the source project")
endif()

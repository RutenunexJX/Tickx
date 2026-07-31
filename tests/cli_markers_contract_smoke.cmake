if(NOT DEFINED WAVE_CLI OR NOT DEFINED PROJECT)
    message(FATAL_ERROR "Markers CLI contract smoke is missing an input")
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
string(JSON compact_marker_query
       GET "${capabilities_json}" features compactMarkerQuery)
set(has_markers FALSE)
math(EXPR command_last "${command_count} - 1")
foreach(command_index RANGE 0 ${command_last})
    string(JSON command_name
           GET "${capabilities_json}" commands ${command_index} name)
    if(command_name STREQUAL "markers")
        set(has_markers TRUE)
    endif()
endforeach()
if(NOT command_count EQUAL 11
   OR NOT compact_marker_query
   OR NOT has_markers)
    message(FATAL_ERROR "capabilities does not advertise the markers contract")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" markers "${PROJECT}"
            "--match=REQUEST" --kind=phase
            "--start=90 ns" "--end=120 ns"
            --scenario=scenario-handshake
    RESULT_VARIABLE marker_code
    OUTPUT_VARIABLE marker_json
    ERROR_VARIABLE marker_error
)
if(NOT marker_code EQUAL 0)
    message(FATAL_ERROR "Marker query failed: ${marker_error}")
endif()
string(JSON marker_command GET "${marker_json}" command)
string(JSON range_filtered GET "${marker_json}" rangeFiltered)
string(JSON start_tick GET "${marker_json}" startTick)
string(JSON end_tick GET "${marker_json}" endTick)
string(JSON scenario_count GET "${marker_json}" scenarioMarkerCount)
string(JSON match_count GET "${marker_json}" matchCount)
string(JSON valid_count GET "${marker_json}" validCount)
string(JSON addressable_count GET "${marker_json}" addressableCount)
string(JSON issue_count GET "${marker_json}" issueCount)
string(JSON marker_id GET "${marker_json}" markers 0 markerId)
string(JSON marker_name GET "${marker_json}" markers 0 name)
string(JSON marker_kind GET "${marker_json}" markers 0 kind)
string(JSON marker_start GET "${marker_json}" markers 0 startTick)
string(JSON marker_end GET "${marker_json}" markers 0 endTick)
string(JSON marker_duration GET "${marker_json}" markers 0 durationTick)
string(JSON marker_point GET "${marker_json}" markers 0 point)
string(JSON marker_valid GET "${marker_json}" markers 0 valid)
string(JSON marker_addressable GET "${marker_json}" markers 0 addressable)
string(JSON marker_issues_length
       LENGTH "${marker_json}" markers 0 issues)
string(JSON marker_row GET "${marker_json}" markers 0)
string(FIND "${marker_row}" "\"extensions\"" extensions_position)
if(NOT marker_command STREQUAL "markers"
   OR NOT range_filtered
   OR NOT start_tick STREQUAL "90000"
   OR NOT end_tick STREQUAL "120000"
   OR NOT scenario_count EQUAL 1
   OR NOT match_count EQUAL 1
   OR NOT valid_count EQUAL 1
   OR NOT addressable_count EQUAL 1
   OR NOT issue_count EQUAL 0
   OR NOT marker_id STREQUAL "marker-transfer"
   OR NOT marker_name STREQUAL "Transfer"
   OR NOT marker_kind STREQUAL "phase"
   OR NOT marker_start STREQUAL "80000"
   OR NOT marker_end STREQUAL "150000"
   OR NOT marker_duration STREQUAL "70000"
   OR marker_point
   OR NOT marker_valid
   OR NOT marker_addressable
   OR NOT marker_issues_length EQUAL 0
   OR NOT extensions_position EQUAL -1)
    message(FATAL_ERROR "Marker query returned the wrong compact result")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" markers "${PROJECT}"
            --match=MARKER-TRANSFER --exact
    RESULT_VARIABLE exact_code
    OUTPUT_VARIABLE exact_json
    ERROR_VARIABLE exact_error
)
if(NOT exact_code EQUAL 0)
    message(FATAL_ERROR "exact Marker query failed: ${exact_error}")
endif()
string(JSON exact_match_count GET "${exact_json}" matchCount)
string(JSON exact_marker_id GET "${exact_json}" markers 0 markerId)
if(NOT exact_match_count EQUAL 1
   OR NOT exact_marker_id STREQUAL "marker-transfer")
    message(FATAL_ERROR "exact Marker query did not match the stable ID")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" markers "${PROJECT}"
            "--start=cycle 9" "--end=cycle 12" --clock=clk
    RESULT_VARIABLE cycle_code
    OUTPUT_VARIABLE cycle_json
    ERROR_VARIABLE cycle_error
)
if(NOT cycle_code EQUAL 0)
    message(FATAL_ERROR "cycle Marker query failed: ${cycle_error}")
endif()
string(JSON cycle_start GET "${cycle_json}" startTick)
string(JSON cycle_end GET "${cycle_json}" endTick)
string(JSON cycle_clock GET "${cycle_json}" clockId)
string(JSON cycle_match_count GET "${cycle_json}" matchCount)
if(NOT cycle_start STREQUAL "90000"
   OR NOT cycle_end STREQUAL "120000"
   OR NOT cycle_clock STREQUAL "clock-main"
   OR NOT cycle_match_count EQUAL 1)
    message(FATAL_ERROR "cycle Marker query used the wrong range")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" markers "${PROJECT}"
            "--start=150 ns" "--end=160 ns"
    RESULT_VARIABLE boundary_code
    OUTPUT_VARIABLE boundary_json
    ERROR_VARIABLE boundary_error
)
if(NOT boundary_code EQUAL 0)
    message(FATAL_ERROR "boundary Marker query failed: ${boundary_error}")
endif()
string(JSON boundary_match_count GET "${boundary_json}" matchCount)
if(NOT boundary_match_count EQUAL 0)
    message(FATAL_ERROR "Marker ending at the half-open boundary was returned")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" markers "${PROJECT}" --kind=warning
    RESULT_VARIABLE kind_code
    OUTPUT_VARIABLE kind_output
    ERROR_VARIABLE kind_json
)
if(NOT kind_code EQUAL 2 OR NOT kind_output STREQUAL "")
    message(FATAL_ERROR "invalid Marker kind was accepted")
endif()
string(JSON kind_error GET "${kind_json}" error code)
if(NOT kind_error STREQUAL "usage")
    message(FATAL_ERROR "invalid Marker kind was not structured")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" markers "${PROJECT}" --exact
    RESULT_VARIABLE exact_usage_code
    OUTPUT_VARIABLE exact_usage_output
    ERROR_VARIABLE exact_usage_json
)
if(NOT exact_usage_code EQUAL 2 OR NOT exact_usage_output STREQUAL "")
    message(FATAL_ERROR "Marker --exact was accepted without --match")
endif()
string(JSON exact_usage_error GET "${exact_usage_json}" error code)
if(NOT exact_usage_error STREQUAL "usage")
    message(FATAL_ERROR "Marker --exact failure was not structured")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" markers "${PROJECT}"
            "--start=130 ns" "--end=70 ns"
    RESULT_VARIABLE range_code
    OUTPUT_VARIABLE range_output
    ERROR_VARIABLE range_json
)
if(NOT range_code EQUAL 4 OR NOT range_output STREQUAL "")
    message(FATAL_ERROR "invalid Marker range was accepted")
endif()
string(JSON range_error GET "${range_json}" error code)
if(NOT range_error STREQUAL "marker-query-rejected")
    message(FATAL_ERROR "invalid Marker range was not structured")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" markers "${PROJECT}"
            "--start=cycle 1" "--end=cycle 2"
    RESULT_VARIABLE clock_code
    OUTPUT_VARIABLE clock_json
    ERROR_VARIABLE clock_error
)
if(NOT clock_code EQUAL 0)
    message(FATAL_ERROR "single-Clock Marker query failed: ${clock_error}")
endif()
string(JSON inferred_clock GET "${clock_json}" clockId)
string(JSON inferred_match_count GET "${clock_json}" matchCount)
if(NOT inferred_clock STREQUAL "clock-main"
   OR NOT inferred_match_count EQUAL 0)
    message(FATAL_ERROR "single-Clock Marker query did not report its inferred Clock")
endif()

file(SHA256 "${PROJECT}" source_sha_after)
if(NOT source_sha_after STREQUAL source_sha_before)
    message(FATAL_ERROR "read-only Marker queries changed the source project")
endif()

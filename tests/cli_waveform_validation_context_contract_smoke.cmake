if(NOT DEFINED WAVE_CLI
   OR NOT DEFINED PROJECT
   OR NOT DEFINED OUTPUT)
    message(FATAL_ERROR "Waveform validation context CLI contract smoke is missing an input")
endif()

set(broken_project "${OUTPUT}-broken.wave.json")
set(repaired_project "${OUTPUT}-repaired.wave.json")
set(dry_output "${OUTPUT}-dry.wave.json")
set(operations_file "${OUTPUT}.operations.json")
file(REMOVE
    "${broken_project}"
    "${repaired_project}"
    "${dry_output}"
    "${operations_file}")

file(SHA256 "${PROJECT}" source_sha_before)
file(READ "${PROJECT}" source_json)
string(REPLACE
    "\"id\": \"segment-data-payload\", \"startTick\": \"80000\", \"endTick\": \"150000\", \"value\": \"0x35\""
    "\"id\": \"segment-data-payload\", \"startTick\": \"80000\", \"endTick\": \"150000\", \"value\": \"0x1ff\""
    broken_json
    "${source_json}")
if(broken_json STREQUAL source_json)
    message(FATAL_ERROR "Waveform validation context fixture was not damaged")
endif()
file(WRITE "${broken_project}" "${broken_json}")
file(SHA256 "${broken_project}" broken_sha_before)

execute_process(
    COMMAND "${WAVE_CLI}" capabilities
    RESULT_VARIABLE capabilities_code
    OUTPUT_VARIABLE capabilities_json
    ERROR_VARIABLE capabilities_error
)
if(NOT capabilities_code EQUAL 0)
    message(FATAL_ERROR "capabilities failed: ${capabilities_error}")
endif()
string(JSON structured_capability
       GET "${capabilities_json}" features structuredWaveformValidation)
if(NOT structured_capability)
    message(FATAL_ERROR "capabilities omit structured waveform validation")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" validate "${broken_project}"
    RESULT_VARIABLE validation_code
    OUTPUT_VARIABLE validation_json
    ERROR_VARIABLE validation_error
)
if(NOT validation_code EQUAL 4)
    message(FATAL_ERROR "Invalid Bus value did not reject validate: ${validation_error}")
endif()

set(waveform_issue_index "")
string(JSON issue_count LENGTH "${validation_json}" issues)
math(EXPR last_issue "${issue_count} - 1")
foreach(issue_index RANGE 0 ${last_issue})
    string(JSON issue_code
           GET "${validation_json}" issues ${issue_index} code)
    if(issue_code STREQUAL "invalid-bus-value")
        set(waveform_issue_index "${issue_index}")
        break()
    endif()
endforeach()
if(waveform_issue_index STREQUAL "")
    message(FATAL_ERROR "validate omitted the invalid Bus value issue")
endif()

string(JSON object_kind
       GET "${validation_json}" issues ${waveform_issue_index} objectKind)
string(JSON scenario_index
       GET "${validation_json}" issues ${waveform_issue_index} scenarioIndex)
string(JSON lane_index
       GET "${validation_json}" issues ${waveform_issue_index} laneIndex)
string(JSON object_index
       GET "${validation_json}" issues ${waveform_issue_index} objectIndex)
string(JSON issue_path
       GET "${validation_json}" issues ${waveform_issue_index} path)
string(JSON repair_property
       GET "${validation_json}" issues ${waveform_issue_index}
           repairProperties 0)
string(JSON first_operation
       GET "${validation_json}" issues ${waveform_issue_index}
           repairOperations 0)
string(JSON second_operation
       GET "${validation_json}" issues ${waveform_issue_index}
           repairOperations 1)
string(JSON lane_id
       GET "${validation_json}" issues ${waveform_issue_index}
           repairRange laneId)
string(JSON start_tick
       GET "${validation_json}" issues ${waveform_issue_index}
           repairRange startTick)
string(JSON end_tick
       GET "${validation_json}" issues ${waveform_issue_index}
           repairRange endTick)
string(JSON lane_name
       GET "${validation_json}" issues ${waveform_issue_index}
           waveformContext laneName)
string(JSON lane_kind
       GET "${validation_json}" issues ${waveform_issue_index}
           waveformContext kind)
string(JSON lane_width
       GET "${validation_json}" issues ${waveform_issue_index}
           waveformContext width)
string(JSON lane_radix
       GET "${validation_json}" issues ${waveform_issue_index}
           waveformContext radix)
string(JSON segment_id
       GET "${validation_json}" issues ${waveform_issue_index}
           waveformContext segmentId)
string(JSON segment_value
       GET "${validation_json}" issues ${waveform_issue_index}
           waveformContext value)
string(JSON formatted_start
       GET "${validation_json}" issues ${waveform_issue_index}
           waveformContext start)
string(JSON formatted_end
       GET "${validation_json}" issues ${waveform_issue_index}
           waveformContext end)
string(JSON undefined
       GET "${validation_json}" issues ${waveform_issue_index}
           waveformContext undefined)
string(JSON repairable_count
       GET "${validation_json}" repairableIssueCount)
string(JSON repair_target_count
       GET "${validation_json}" repairTargetCount)
if(NOT object_kind STREQUAL "segment"
   OR NOT scenario_index EQUAL 0
   OR NOT lane_index EQUAL 5
   OR NOT object_index EQUAL 1
   OR NOT issue_path STREQUAL "scenarios[0].lanes[5].segments[1].value"
   OR NOT repair_property STREQUAL "value"
   OR NOT first_operation STREQUAL "set-range"
   OR NOT second_operation STREQUAL "clear-range"
   OR NOT lane_id STREQUAL "lane-data"
   OR NOT start_tick EQUAL 80000
   OR NOT end_tick EQUAL 150000
   OR NOT lane_name STREQUAL "data[7:0]"
   OR NOT lane_kind STREQUAL "bus"
   OR NOT lane_width EQUAL 8
   OR NOT lane_radix STREQUAL "hexadecimal"
   OR NOT segment_id STREQUAL "segment-data-payload"
   OR NOT segment_value STREQUAL "0x1ff"
   OR NOT formatted_start STREQUAL "80 ns"
   OR NOT formatted_end STREQUAL "150 ns"
   OR undefined
   OR repairable_count LESS 1
   OR repair_target_count LESS 1)
    message(FATAL_ERROR "validate did not return the structured waveform context")
endif()

file(WRITE "${operations_file}"
"{
  \"schema\": \"wave-workbench.operations/v1\",
  \"scenarioId\": \"scenario-handshake\",
  \"operations\": [
    {
      \"op\": \"set-range\",
      \"laneId\": \"${lane_id}\",
      \"startTick\": \"${start_tick}\",
      \"endTick\": \"${end_tick}\",
      \"value\": \"0x35\"
    }
  ]
}
")

execute_process(
    COMMAND "${WAVE_CLI}" apply "${broken_project}" "${operations_file}"
            "--output=${dry_output}" --dry-run
    RESULT_VARIABLE dry_code
    OUTPUT_VARIABLE dry_json
    ERROR_VARIABLE dry_error
)
if(NOT dry_code EQUAL 0 OR EXISTS "${dry_output}")
    message(FATAL_ERROR "Structured waveform dry-run failed or wrote output: ${dry_error}")
endif()
string(JSON dry_sha GET "${dry_json}" resultSha256)
string(JSON dry_source_errors
       GET "${dry_json}" validationGuard sourceErrorCount)
string(JSON dry_candidate_errors
       GET "${dry_json}" validationGuard candidateErrorCount)
string(JSON dry_reason
       GET "${dry_json}" validationGuard reason)
if(NOT dry_source_errors EQUAL 1
   OR NOT dry_candidate_errors EQUAL 0
   OR NOT dry_reason STREQUAL "valid-candidate")
    message(FATAL_ERROR "Structured waveform repair did not remove the value error")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" apply "${broken_project}" "${operations_file}"
            "--output=${repaired_project}"
    RESULT_VARIABLE apply_code
    OUTPUT_VARIABLE apply_json
    ERROR_VARIABLE apply_error
)
if(NOT apply_code EQUAL 0 OR NOT EXISTS "${repaired_project}")
    message(FATAL_ERROR "Structured waveform apply failed: ${apply_error}")
endif()
string(JSON apply_sha GET "${apply_json}" resultSha256)
file(SHA256 "${repaired_project}" repaired_sha)
if(NOT apply_sha STREQUAL dry_sha
   OR NOT repaired_sha STREQUAL apply_sha)
    message(FATAL_ERROR "Structured waveform dry-run and write SHA differ")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" validate "${repaired_project}"
    RESULT_VARIABLE final_code
    OUTPUT_VARIABLE final_json
    ERROR_VARIABLE final_error
)
if(NOT final_code EQUAL 0)
    message(FATAL_ERROR "Structured waveform result did not validate: ${final_error}")
endif()
string(JSON final_valid GET "${final_json}" valid)
string(JSON final_errors GET "${final_json}" summary errors)
string(JSON final_repairable GET "${final_json}" repairableIssueCount)
math(EXPR expected_final_repairable "${repairable_count} - 1")
if(NOT final_valid
   OR NOT final_errors EQUAL 0
   OR NOT final_repairable EQUAL expected_final_repairable)
    message(FATAL_ERROR "Structured waveform repair left validation damage")
endif()

file(SHA256 "${PROJECT}" source_sha_after)
file(SHA256 "${broken_project}" broken_sha_after)
if(NOT source_sha_after STREQUAL source_sha_before
   OR NOT broken_sha_after STREQUAL broken_sha_before)
    message(FATAL_ERROR "Structured waveform workflow changed a source project")
endif()

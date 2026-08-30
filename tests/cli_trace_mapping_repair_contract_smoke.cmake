if(NOT DEFINED WAVE_CLI
   OR NOT DEFINED PROJECT
   OR NOT DEFINED OUTPUT)
    message(FATAL_ERROR "Trace mapping repair CLI contract smoke is missing an input")
endif()

set(broken_project "${OUTPUT}-broken.wave.json")
set(repaired_project "${OUTPUT}-repaired.wave.json")
set(dry_output "${OUTPUT}-dry.wave.json")
set(healthy_output "${OUTPUT}-healthy-output.wave.json")
set(operations_file "${OUTPUT}.operations.json")
set(healthy_operations_file "${OUTPUT}-healthy.operations.json")
file(REMOVE
    "${broken_project}"
    "${repaired_project}"
    "${dry_output}"
    "${healthy_output}"
    "${operations_file}"
    "${healthy_operations_file}")

file(SHA256 "${PROJECT}" source_sha_before)
file(READ "${PROJECT}" source_json)
string(JSON broken_json
       SET "${source_json}" importedTraces 0 signalMapping lane-stale
       "\"tb.dut.removed\"")
string(JSON broken_json
       SET "${broken_json}" importedTraces 0 signalMapping lane-data
       "\"\"")
file(WRITE "${broken_project}" "${broken_json}")
file(SHA256 "${broken_project}" broken_sha_before)

execute_process(
    COMMAND "${WAVE_CLI}" capabilities
    RESULT_VARIABLE capabilities_code
    OUTPUT_VARIABLE capabilities_json
    ERROR_VARIABLE capabilities_error
)
if(NOT capabilities_code EQUAL 0)
    message(FATAL_ERROR
        "capabilities failed (${capabilities_code}): ${capabilities_error}")
endif()
string(JSON mapping_validation
       GET "${capabilities_json}" features traceMappingValidation)
string(JSON mapping_repair
       GET "${capabilities_json}" features traceMappingRepair)
string(JSON trace_reference
       GET "${capabilities_json}" features traceRepairReference)
string(JSON operation_count
       GET "${capabilities_json}" operationCount)
string(JSON operation_length
       LENGTH "${capabilities_json}" operations)
set(has_repair_trace_mapping FALSE)
math(EXPR operation_last "${operation_length} - 1")
foreach(operation_index RANGE 0 ${operation_last})
    string(JSON operation_name
           GET "${capabilities_json}" operations ${operation_index} name)
    if(operation_name STREQUAL "repair-trace-mapping")
        set(has_repair_trace_mapping TRUE)
    endif()
endforeach()
if(NOT mapping_validation
   OR NOT mapping_repair
   OR NOT trace_reference
   OR NOT operation_count EQUAL 38
   OR NOT operation_length EQUAL 38
   OR NOT has_repair_trace_mapping)
    message(FATAL_ERROR "capabilities omit Trace mapping repair")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" validate "${PROJECT}"
    RESULT_VARIABLE baseline_code
    OUTPUT_VARIABLE baseline_json
    ERROR_VARIABLE baseline_error
)
if(NOT baseline_code EQUAL 0)
    message(FATAL_ERROR "Source project did not validate: ${baseline_error}")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" validate "${broken_project}"
    RESULT_VARIABLE validation_code
    OUTPUT_VARIABLE validation_json
    ERROR_VARIABLE validation_error
)
if(NOT validation_code EQUAL 4)
    message(FATAL_ERROR "Invalid Trace mappings did not reject validate: ${validation_error}")
endif()
set(stale_issue_index "")
set(empty_issue_index "")
string(JSON issue_count LENGTH "${validation_json}" issues)
math(EXPR last_issue "${issue_count} - 1")
foreach(issue_index RANGE 0 ${last_issue})
    string(JSON issue_code
           GET "${validation_json}" issues ${issue_index} code)
    string(JSON issue_lane
           GET "${validation_json}" issues ${issue_index} laneId)
    if(issue_code STREQUAL "trace-mapping-invalid"
       AND issue_lane STREQUAL "lane-stale")
        set(stale_issue_index "${issue_index}")
    elseif(issue_code STREQUAL "trace-mapping-invalid"
           AND issue_lane STREQUAL "lane-data")
        set(empty_issue_index "${issue_index}")
    endif()
endforeach()
if(stale_issue_index STREQUAL ""
   OR empty_issue_index STREQUAL "")
    message(FATAL_ERROR "validate omitted invalid Trace mappings")
endif()

string(JSON error_count
       GET "${validation_json}" summary errors)
string(JSON mapping_issue_count
       GET "${validation_json}" traceMappingIssueCount)
string(JSON affected_trace_count
       GET "${validation_json}" traceMappingSummary affectedTraceCount)
string(JSON invalid_mapping_count
       GET "${validation_json}" traceMappingSummary invalidMappingCount)
string(JSON missing_lane_count
       GET "${validation_json}" traceMappingSummary missingLaneReferenceCount)
string(JSON empty_signal_count
       GET "${validation_json}" traceMappingSummary emptyActualSignalIdCount)
string(JSON trace_ref
       GET "${validation_json}" issues ${stale_issue_index} traceRef)
string(JSON trace_id
       GET "${validation_json}" issues ${stale_issue_index} traceId)
string(JSON trace_index
       GET "${validation_json}" issues ${stale_issue_index} traceIndex)
string(JSON trace_id_matches
       GET "${validation_json}" issues ${stale_issue_index}
           traceContext traceIdMatchCount)
string(JSON stale_lane_matches
       GET "${validation_json}" issues ${stale_issue_index}
           traceMappingContext expectedLaneMatchCount)
string(JSON stale_lane_valid
       GET "${validation_json}" issues ${stale_issue_index}
           traceMappingContext expectedLaneReferenceValid)
string(JSON stale_signal_present
       GET "${validation_json}" issues ${stale_issue_index}
           traceMappingContext actualSignalIdPresent)
string(JSON external_verification
       GET "${validation_json}" issues ${stale_issue_index}
           traceMappingContext externalSignalVerification)
string(JSON stale_action
       GET "${validation_json}" issues ${stale_issue_index}
           traceMappingContext repairAction)
string(JSON stale_repairable
       GET "${validation_json}" issues ${stale_issue_index}
           traceMappingContext repairable)
string(JSON stale_problem
       GET "${validation_json}" issues ${stale_issue_index}
           traceMappingContext problems 0)
string(JSON repair_operation
       GET "${validation_json}" issues ${stale_issue_index}
           repairOperations 0)
string(JSON empty_lane_matches
       GET "${validation_json}" issues ${empty_issue_index}
           traceMappingContext expectedLaneMatchCount)
string(JSON empty_signal_present
       GET "${validation_json}" issues ${empty_issue_index}
           traceMappingContext actualSignalIdPresent)
string(JSON empty_problem
       GET "${validation_json}" issues ${empty_issue_index}
           traceMappingContext problems 0)
if(NOT error_count EQUAL 2
   OR NOT mapping_issue_count EQUAL 2
   OR NOT affected_trace_count EQUAL 1
   OR NOT invalid_mapping_count EQUAL 2
   OR NOT missing_lane_count EQUAL 1
   OR NOT empty_signal_count EQUAL 1
   OR trace_ref STREQUAL ""
   OR NOT trace_id STREQUAL "trace-handshake-actual"
   OR NOT trace_index EQUAL 0
   OR NOT trace_id_matches EQUAL 1
   OR NOT stale_lane_matches EQUAL 0
   OR stale_lane_valid
   OR NOT stale_signal_present
   OR NOT external_verification STREQUAL "not-performed"
   OR NOT stale_action STREQUAL "remove-invalid-mapping"
   OR NOT stale_repairable
   OR NOT stale_problem STREQUAL "expected-lane-not-found"
   OR NOT repair_operation STREQUAL "repair-trace-mapping"
   OR NOT empty_lane_matches EQUAL 1
   OR empty_signal_present
   OR NOT empty_problem STREQUAL "empty-actual-signal-id")
    message(FATAL_ERROR "validate returned the wrong Trace mapping repair context")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" inspect "${broken_project}"
    RESULT_VARIABLE inspect_code
    OUTPUT_VARIABLE inspect_json
    ERROR_VARIABLE inspect_error
)
if(NOT inspect_code EQUAL 0)
    message(FATAL_ERROR "full inspect failed: ${inspect_error}")
endif()
string(JSON inspect_trace_ref
       GET "${inspect_json}" project importedTraces 0 traceRef)
string(JSON inspect_mapping
       GET "${inspect_json}" project importedTraces 0 signalMapping lane-stale)
if(NOT inspect_trace_ref STREQUAL trace_ref
   OR NOT inspect_mapping STREQUAL "tb.dut.removed")
    message(FATAL_ERROR "full inspect omitted Trace mapping detail")
endif()

file(WRITE "${operations_file}"
"{
  \"schema\": \"wave-workbench.operations/v1\",
  \"scenarioId\": \"scenario-handshake\",
  \"operations\": [
    {
      \"op\": \"repair-trace-mapping\",
      \"traceRef\": \"${trace_ref}\",
      \"laneId\": \"lane-stale\"
    },
    {
      \"op\": \"repair-trace-mapping\",
      \"traceId\": \"${trace_id}\",
      \"laneId\": \"lane-data\"
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
    message(FATAL_ERROR "Trace mapping repair dry-run failed or wrote output: ${dry_error}")
endif()
string(JSON dry_sha GET "${dry_json}" resultSha256)
string(JSON source_errors
       GET "${dry_json}" validationGuard sourceErrorCount)
string(JSON candidate_errors
       GET "${dry_json}" validationGuard candidateErrorCount)
string(JSON guard_reason
       GET "${dry_json}" validationGuard reason)
string(JSON changes
       GET "${dry_json}" changes importedTracesChanged)
string(JSON first_removed
       GET "${dry_json}" operations 0 removedTraceMappingCount)
string(JSON second_removed
       GET "${dry_json}" operations 1 removedTraceMappingCount)
string(JSON first_before_count
       GET "${dry_json}" operations 0 beforeMappingCount)
string(JSON first_after_count
       GET "${dry_json}" operations 0 mappingCount)
string(JSON second_before_count
       GET "${dry_json}" operations 1 beforeMappingCount)
string(JSON second_after_count
       GET "${dry_json}" operations 1 mappingCount)
string(JSON first_action
       GET "${dry_json}" operations 0
           beforeTraceMappingContext repairAction)
string(JSON final_mapping_count
       GET "${dry_json}" operations 1 traceContext mappingCount)
if(NOT source_errors EQUAL 2
   OR NOT candidate_errors EQUAL 0
   OR NOT guard_reason STREQUAL "valid-candidate"
   OR NOT changes
   OR NOT first_removed EQUAL 1
   OR NOT second_removed EQUAL 1
   OR NOT first_before_count EQUAL 7
   OR NOT first_after_count EQUAL 6
   OR NOT second_before_count EQUAL 6
   OR NOT second_after_count EQUAL 5
   OR NOT first_action STREQUAL "remove-invalid-mapping"
   OR NOT final_mapping_count EQUAL 5)
    message(FATAL_ERROR "Trace mapping repair changed unrelated project data")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" apply "${broken_project}" "${operations_file}"
            "--output=${repaired_project}"
    RESULT_VARIABLE apply_code
    OUTPUT_VARIABLE apply_json
    ERROR_VARIABLE apply_error
)
if(NOT apply_code EQUAL 0 OR NOT EXISTS "${repaired_project}")
    message(FATAL_ERROR "Trace mapping repair failed: ${apply_error}")
endif()
string(JSON apply_sha GET "${apply_json}" resultSha256)
file(SHA256 "${repaired_project}" repaired_sha)
if(NOT apply_sha STREQUAL dry_sha
   OR NOT repaired_sha STREQUAL apply_sha)
    message(FATAL_ERROR "Trace mapping repair dry-run and write SHA differ")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" validate "${repaired_project}"
    RESULT_VARIABLE final_code
    OUTPUT_VARIABLE final_json
    ERROR_VARIABLE final_error
)
if(NOT final_code EQUAL 0)
    message(FATAL_ERROR "Trace mapping repair result did not validate: ${final_error}")
endif()
string(JSON final_valid GET "${final_json}" valid)
string(JSON final_errors GET "${final_json}" summary errors)
file(READ "${repaired_project}" repaired_json)
string(JSON repaired_mapping_count
       LENGTH "${repaired_json}" importedTraces 0 signalMapping)
string(JSON retained_mapping
       GET "${repaired_json}" importedTraces 0 signalMapping lane-request)
string(JSON removed_stale
       ERROR_VARIABLE removed_stale_error
       GET "${repaired_json}" importedTraces 0 signalMapping lane-stale)
string(JSON removed_empty
       ERROR_VARIABLE removed_empty_error
       GET "${repaired_json}" importedTraces 0 signalMapping lane-data)
if(NOT final_valid
   OR NOT final_errors EQUAL 0
   OR NOT repaired_mapping_count EQUAL 5
   OR NOT retained_mapping STREQUAL "tb.dut.req"
   OR removed_stale_error STREQUAL "NOTFOUND"
   OR removed_empty_error STREQUAL "NOTFOUND")
    message(FATAL_ERROR "Trace mapping repair did not preserve valid mappings")
endif()

file(WRITE "${healthy_operations_file}"
"{
  \"schema\": \"wave-workbench.operations/v1\",
  \"scenarioId\": \"scenario-handshake\",
  \"operations\": [
    {
      \"op\": \"repair-trace-mapping\",
      \"traceId\": \"trace-handshake-actual\",
      \"laneId\": \"lane-request\"
    }
  ]
}
")
execute_process(
    COMMAND "${WAVE_CLI}" apply "${PROJECT}" "${healthy_operations_file}"
            "--output=${healthy_output}"
    RESULT_VARIABLE healthy_code
    OUTPUT_VARIABLE healthy_stdout
    ERROR_VARIABLE healthy_error
)
if(NOT healthy_code EQUAL 4
   OR NOT healthy_stdout STREQUAL ""
   OR EXISTS "${healthy_output}")
    message(FATAL_ERROR "Healthy Trace mapping repair was not atomically rejected")
endif()
string(FIND "${healthy_error}" "not reported by validate" healthy_error_position)
if(healthy_error_position EQUAL -1)
    message(FATAL_ERROR "Healthy Trace mapping repair omitted its validation-only cause")
endif()

file(SHA256 "${PROJECT}" source_sha_after)
file(SHA256 "${broken_project}" broken_sha_after)
if(NOT source_sha_after STREQUAL source_sha_before
   OR NOT broken_sha_after STREQUAL broken_sha_before)
    message(FATAL_ERROR "Trace mapping repair workflow changed a source project")
endif()

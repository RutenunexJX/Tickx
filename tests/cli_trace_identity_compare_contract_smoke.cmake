if(NOT DEFINED WAVE_CLI
   OR NOT DEFINED WAVE_COMPARE
   OR NOT DEFINED PROJECT
   OR NOT DEFINED OUTPUT)
    message(FATAL_ERROR "Trace identity/compare CLI contract smoke is missing an input")
endif()

set(duplicate_project "${OUTPUT}-duplicate.wave.json")
set(repaired_project "${OUTPUT}-repaired.wave.json")
set(dry_output "${OUTPUT}-dry.wave.json")
set(stale_mapping_project "${OUTPUT}-stale-mapping.wave.json")
set(unmapped_project "${OUTPUT}-unmapped.wave.json")
set(operations_file "${OUTPUT}.operations.json")
set(duplicate_compare_dir "${OUTPUT}-duplicate-compare")
set(repaired_compare_dir "${OUTPUT}-repaired-compare")
set(stale_compare_dir "${OUTPUT}-stale-compare")
set(unmapped_compare_dir "${OUTPUT}-unmapped-compare")
file(REMOVE
    "${duplicate_project}"
    "${repaired_project}"
    "${dry_output}"
    "${stale_mapping_project}"
    "${unmapped_project}"
    "${operations_file}")
file(REMOVE_RECURSE
    "${duplicate_compare_dir}"
    "${repaired_compare_dir}"
    "${stale_compare_dir}"
    "${unmapped_compare_dir}")

file(SHA256 "${PROJECT}" source_sha_before)
file(READ "${PROJECT}" source_json)
get_filename_component(project_directory "${PROJECT}" DIRECTORY)
file(TO_CMAKE_PATH
    "${project_directory}/traces/handshake_actual.vcd"
    trace_path)
string(JSON source_with_absolute_trace
       SET "${source_json}" importedTraces 0 path
       "\"${trace_path}\"")
string(JSON first_trace
       GET "${source_with_absolute_trace}" importedTraces 0)
string(JSON duplicate_json
       SET "${source_with_absolute_trace}" importedTraces 1
       "${first_trace}")
file(WRITE "${duplicate_project}" "${duplicate_json}")
file(SHA256 "${duplicate_project}" duplicate_sha_before)

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
string(JSON identity_validation
       GET "${capabilities_json}" features traceIdentityValidation)
string(JSON identity_repair
       GET "${capabilities_json}" features traceIdentityRepair)
string(JSON operation_count
       GET "${capabilities_json}" operationCount)
string(JSON operation_length
       LENGTH "${capabilities_json}" operations)
set(has_repair_trace_identity FALSE)
math(EXPR operation_last "${operation_length} - 1")
foreach(operation_index RANGE 0 ${operation_last})
    string(JSON operation_name
           GET "${capabilities_json}" operations ${operation_index} name)
    if(operation_name STREQUAL "repair-trace-identity")
        set(has_repair_trace_identity TRUE)
    endif()
endforeach()
if(NOT identity_validation
   OR NOT identity_repair
   OR NOT operation_count EQUAL 33
   OR NOT operation_length EQUAL 33
   OR NOT has_repair_trace_identity)
    message(FATAL_ERROR "capabilities omit Imported Trace identity repair")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" validate "${duplicate_project}"
    RESULT_VARIABLE validation_code
    OUTPUT_VARIABLE validation_json
    ERROR_VARIABLE validation_error
)
if(NOT validation_code EQUAL 4)
    message(FATAL_ERROR
        "Duplicate Imported Trace IDs did not reject validate: ${validation_error}")
endif()
string(JSON identity_issue_count
       GET "${validation_json}" identityIssueCount)
string(JSON repairable_issue_count
       GET "${validation_json}" repairableIssueCount)
string(JSON repair_target_count
       GET "${validation_json}" repairTargetCount)
string(JSON issue_count LENGTH "${validation_json}" issues)
set(first_trace_ref "")
set(second_trace_ref "")
math(EXPR issue_last "${issue_count} - 1")
foreach(issue_index RANGE 0 ${issue_last})
    string(JSON issue_code
           GET "${validation_json}" issues ${issue_index} code)
    string(JSON issue_kind
           ERROR_VARIABLE issue_kind_error
           GET "${validation_json}" issues ${issue_index} objectKind)
    if(issue_kind_error)
        set(issue_kind "")
    endif()
    if(issue_code STREQUAL "duplicate-stable-id"
       AND issue_kind STREQUAL "imported-trace")
        string(JSON issue_trace_index
               GET "${validation_json}" issues ${issue_index} traceIndex)
        string(JSON issue_trace_ref
               GET "${validation_json}" issues ${issue_index} traceRef)
        string(JSON issue_trace_matches
               GET "${validation_json}" issues ${issue_index}
                   traceContext traceIdMatchCount)
        string(JSON issue_repair
               GET "${validation_json}" issues ${issue_index}
                   repairOperations 0)
        if(NOT issue_trace_matches EQUAL 2
           OR NOT issue_repair STREQUAL "repair-trace-identity")
            message(FATAL_ERROR
                "Imported Trace identity issue omitted repair context")
        endif()
        if(issue_trace_index EQUAL 0)
            set(first_trace_ref "${issue_trace_ref}")
        elseif(issue_trace_index EQUAL 1)
            set(second_trace_ref "${issue_trace_ref}")
        endif()
    endif()
endforeach()
if(NOT identity_issue_count EQUAL 2
   OR repairable_issue_count LESS 2
   OR repair_target_count LESS 2
   OR first_trace_ref STREQUAL ""
   OR second_trace_ref STREQUAL ""
   OR first_trace_ref STREQUAL second_trace_ref)
    message(FATAL_ERROR
        "validate did not identify both duplicate Imported Trace snapshots")
endif()

execute_process(
    COMMAND "${WAVE_COMPARE}" "${duplicate_project}"
            "${duplicate_compare_dir}"
    RESULT_VARIABLE implicit_duplicate_code
    OUTPUT_VARIABLE implicit_duplicate_output
    ERROR_VARIABLE implicit_duplicate_error
)
if(NOT implicit_duplicate_code EQUAL 2
   OR NOT implicit_duplicate_error MATCHES "specify --trace-id")
    message(FATAL_ERROR
        "wave-compare silently selected the first of multiple traces")
endif()
execute_process(
    COMMAND "${WAVE_COMPARE}" "${duplicate_project}"
            "${duplicate_compare_dir}"
            "--trace-id=trace-handshake-actual"
    RESULT_VARIABLE explicit_duplicate_code
    OUTPUT_VARIABLE explicit_duplicate_output
    ERROR_VARIABLE explicit_duplicate_error
)
if(NOT explicit_duplicate_code EQUAL 2
   OR NOT explicit_duplicate_error MATCHES "ambiguous")
    message(FATAL_ERROR
        "wave-compare silently selected a duplicate Trace ID")
endif()

file(WRITE "${operations_file}"
"{
  \"schema\": \"wave-workbench.operations/v1\",
  \"scenarioId\": \"scenario-handshake\",
  \"operations\": [
    {
      \"op\": \"repair-trace-identity\",
      \"traceRef\": \"${second_trace_ref}\"
    }
  ]
}
")

execute_process(
    COMMAND "${WAVE_CLI}" apply "${duplicate_project}" "${operations_file}"
            "--output=${dry_output}" --dry-run
    RESULT_VARIABLE dry_code
    OUTPUT_VARIABLE dry_json
    ERROR_VARIABLE dry_error
)
if(NOT dry_code EQUAL 0 OR EXISTS "${dry_output}")
    message(FATAL_ERROR
        "Imported Trace identity dry-run failed or wrote output: ${dry_error}")
endif()
string(JSON dry_trace_id
       GET "${dry_json}" operations 0 traceId)
string(JSON dry_generated
       GET "${dry_json}" operations 0 generatedId)
string(JSON dry_before_matches
       GET "${dry_json}" operations 0 beforeTraceIdMatchCount)
string(JSON dry_updated
       GET "${dry_json}" operations 0 updatedTraceCount)
string(JSON dry_source_errors
       GET "${dry_json}" validationGuard sourceErrorCount)
string(JSON dry_candidate_errors
       GET "${dry_json}" validationGuard candidateErrorCount)
if(NOT dry_generated
   OR NOT dry_before_matches EQUAL 2
   OR NOT dry_updated EQUAL 1
   OR NOT dry_source_errors EQUAL 2
   OR NOT dry_candidate_errors EQUAL 0
   OR NOT dry_trace_id MATCHES "^trace-auto-")
    message(FATAL_ERROR
        "Imported Trace identity dry-run returned the wrong repair result")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" apply "${duplicate_project}" "${operations_file}"
            "--output=${repaired_project}"
    RESULT_VARIABLE apply_code
    OUTPUT_VARIABLE apply_json
    ERROR_VARIABLE apply_error
)
if(NOT apply_code EQUAL 0 OR NOT EXISTS "${repaired_project}")
    message(FATAL_ERROR
        "Imported Trace identity repair failed: ${apply_error}")
endif()
string(JSON repaired_trace_id
       GET "${apply_json}" operations 0 traceId)
string(JSON before_path
       GET "${duplicate_json}" importedTraces 1 path)
string(JSON before_format
       GET "${duplicate_json}" importedTraces 1 format)
string(JSON before_offset
       GET "${duplicate_json}" importedTraces 1 offsetTick)
file(READ "${repaired_project}" repaired_json)
string(JSON after_first_id
       GET "${repaired_json}" importedTraces 0 id)
string(JSON after_second_id
       GET "${repaired_json}" importedTraces 1 id)
string(JSON after_path
       GET "${repaired_json}" importedTraces 1 path)
string(JSON after_format
       GET "${repaired_json}" importedTraces 1 format)
string(JSON after_offset
       GET "${repaired_json}" importedTraces 1 offsetTick)
if(NOT repaired_trace_id STREQUAL dry_trace_id
   OR NOT after_first_id STREQUAL "trace-handshake-actual"
   OR NOT after_second_id STREQUAL repaired_trace_id
   OR NOT after_path STREQUAL before_path
   OR NOT after_format STREQUAL before_format
   OR NOT after_offset STREQUAL before_offset)
    message(FATAL_ERROR
        "Imported Trace identity repair changed unrelated Trace state")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" validate "${repaired_project}"
    RESULT_VARIABLE repaired_validation_code
    OUTPUT_VARIABLE repaired_validation_json
    ERROR_VARIABLE repaired_validation_error
)
if(NOT repaired_validation_code EQUAL 0)
    message(FATAL_ERROR
        "Repaired Imported Trace project did not validate: ${repaired_validation_error}")
endif()
string(JSON repaired_identity_issues
       GET "${repaired_validation_json}" identityIssueCount)
if(NOT repaired_identity_issues EQUAL 0)
    message(FATAL_ERROR
        "Repaired project retained Imported Trace identity issues")
endif()

execute_process(
    COMMAND "${WAVE_COMPARE}" "${repaired_project}"
            "${repaired_compare_dir}"
    RESULT_VARIABLE repaired_implicit_code
    OUTPUT_VARIABLE repaired_implicit_output
    ERROR_VARIABLE repaired_implicit_error
)
if(NOT repaired_implicit_code EQUAL 2
   OR NOT repaired_implicit_error MATCHES "specify --trace-id")
    message(FATAL_ERROR
        "wave-compare did not require an explicit choice for multiple valid traces")
endif()
execute_process(
    COMMAND "${WAVE_COMPARE}" "${repaired_project}"
            "${repaired_compare_dir}"
            "--trace-id=trace-handshake-actual"
    RESULT_VARIABLE first_compare_code
    OUTPUT_VARIABLE first_compare_output
    ERROR_VARIABLE first_compare_error
)
if(NOT first_compare_code EQUAL 0)
    message(FATAL_ERROR
        "wave-compare rejected the first unique Trace: ${first_compare_error}")
endif()
execute_process(
    COMMAND "${WAVE_COMPARE}" "${repaired_project}"
            "${repaired_compare_dir}"
            "--trace-id=${repaired_trace_id}"
    RESULT_VARIABLE second_compare_code
    OUTPUT_VARIABLE second_compare_output
    ERROR_VARIABLE second_compare_error
)
if(NOT second_compare_code EQUAL 0)
    message(FATAL_ERROR
        "wave-compare rejected the repaired unique Trace: ${second_compare_error}")
endif()

string(JSON stale_mapping_json
       SET "${source_with_absolute_trace}"
           importedTraces 0 signalMapping lane-request
       "\"tb.dut.removed_request\"")
file(WRITE "${stale_mapping_project}" "${stale_mapping_json}")
execute_process(
    COMMAND "${WAVE_COMPARE}" "${stale_mapping_project}"
            "${stale_compare_dir}"
    RESULT_VARIABLE stale_compare_code
    OUTPUT_VARIABLE stale_compare_output
    ERROR_VARIABLE stale_compare_error
)
if(NOT stale_compare_code EQUAL 0)
    message(FATAL_ERROR
        "wave-compare failed on a stale mapping diagnostic: ${stale_compare_error}")
endif()
file(GLOB stale_reports "${stale_compare_dir}/*.compare.json")
list(LENGTH stale_reports stale_report_count)
if(NOT stale_report_count EQUAL 1)
    message(FATAL_ERROR "wave-compare did not write one stale-mapping report")
endif()
list(GET stale_reports 0 stale_report)
file(READ "${stale_report}" stale_report_json)
string(JSON stale_difference_count
       LENGTH "${stale_report_json}" differences)
set(found_stale_mapping FALSE)
math(EXPR stale_difference_last "${stale_difference_count} - 1")
foreach(difference_index RANGE 0 ${stale_difference_last})
    string(JSON difference_lane
           GET "${stale_report_json}" differences ${difference_index} laneId)
    string(JSON difference_kind
           GET "${stale_report_json}" differences ${difference_index} kind)
    string(JSON difference_signal
           GET "${stale_report_json}" differences ${difference_index}
               traceSignalId)
    string(JSON difference_message
           GET "${stale_report_json}" differences ${difference_index} message)
    if(difference_lane STREQUAL "lane-request"
       AND difference_kind STREQUAL "missing-signal"
       AND difference_signal STREQUAL "tb.dut.removed_request"
       AND difference_message MATCHES "does not exist in the loaded trace")
        set(found_stale_mapping TRUE)
    endif()
endforeach()
if(NOT found_stale_mapping)
    message(FATAL_ERROR
        "Compare report did not identify the missing configured signal")
endif()

string(JSON unmapped_json
       REMOVE "${source_with_absolute_trace}"
              importedTraces 0 signalMapping lane-request)
file(WRITE "${unmapped_project}" "${unmapped_json}")
execute_process(
    COMMAND "${WAVE_COMPARE}" "${unmapped_project}"
            "${unmapped_compare_dir}"
    RESULT_VARIABLE unmapped_compare_code
    OUTPUT_VARIABLE unmapped_compare_output
    ERROR_VARIABLE unmapped_compare_error
)
if(NOT unmapped_compare_code EQUAL 0)
    message(FATAL_ERROR
        "wave-compare failed on an unmapped Lane diagnostic: ${unmapped_compare_error}")
endif()
file(GLOB unmapped_reports "${unmapped_compare_dir}/*.compare.json")
list(LENGTH unmapped_reports unmapped_report_count)
if(NOT unmapped_report_count EQUAL 1)
    message(FATAL_ERROR "wave-compare did not write one unmapped report")
endif()
list(GET unmapped_reports 0 unmapped_report)
file(READ "${unmapped_report}" unmapped_report_json)
string(JSON unmapped_difference_count
       LENGTH "${unmapped_report_json}" differences)
set(found_unmapped FALSE)
math(EXPR unmapped_difference_last "${unmapped_difference_count} - 1")
foreach(difference_index RANGE 0 ${unmapped_difference_last})
    string(JSON difference_lane
           GET "${unmapped_report_json}" differences ${difference_index} laneId)
    string(JSON difference_kind
           GET "${unmapped_report_json}" differences ${difference_index} kind)
    string(JSON difference_signal
           GET "${unmapped_report_json}" differences ${difference_index}
               traceSignalId)
    if(difference_lane STREQUAL "lane-request"
       AND difference_kind STREQUAL "unmapped-signal"
       AND difference_signal STREQUAL "")
        set(found_unmapped TRUE)
    endif()
endforeach()
if(NOT found_unmapped)
    message(FATAL_ERROR
        "Compare report did not distinguish an unconfigured mapping")
endif()

file(SHA256 "${PROJECT}" source_sha_after)
file(SHA256 "${duplicate_project}" duplicate_sha_after)
if(NOT source_sha_before STREQUAL source_sha_after
   OR NOT duplicate_sha_before STREQUAL duplicate_sha_after)
    message(FATAL_ERROR
        "Trace identity/compare contract modified an input project")
endif()

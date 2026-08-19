if(NOT DEFINED WAVE_CLI
   OR NOT DEFINED WAVE_COMPARE
   OR NOT DEFINED PROJECT
   OR NOT DEFINED OUTPUT)
    message(FATAL_ERROR
        "Trace reference repair CLI contract smoke is missing an input")
endif()

set(broken_project
    "${OUTPUT}-broken.wave.json")
set(bad_format_project
    "${OUTPUT}-bad-format.wave.json")
set(stale_project
    "${OUTPUT}-stale.wave.json")
set(repaired_project
    "${OUTPUT}-repaired.wave.json")
set(dry_output
    "${OUTPUT}-dry.wave.json")
set(rejected_output
    "${OUTPUT}-rejected.wave.json")
set(operations_file
    "${OUTPUT}.operations.json")
set(partial_operations_file
    "${OUTPUT}-partial.operations.json")
set(healthy_operations_file
    "${OUTPUT}-healthy.operations.json")
set(broken_compare_dir
    "${OUTPUT}-broken-compare")
set(bad_format_compare_dir
    "${OUTPUT}-bad-format-compare")
set(repaired_compare_dir
    "${OUTPUT}-repaired-compare")
file(REMOVE
    "${broken_project}"
    "${bad_format_project}"
    "${stale_project}"
    "${repaired_project}"
    "${dry_output}"
    "${rejected_output}"
    "${operations_file}"
    "${partial_operations_file}"
    "${healthy_operations_file}")
file(REMOVE_RECURSE
    "${broken_compare_dir}"
    "${bad_format_compare_dir}"
    "${repaired_compare_dir}")

file(SHA256 "${PROJECT}" source_sha_before)
file(READ "${PROJECT}" source_json)
get_filename_component(
    project_directory "${PROJECT}" DIRECTORY)
file(TO_CMAKE_PATH
    "${project_directory}/traces/handshake_actual.vcd"
    trace_path)

string(JSON broken_json
       SET "${source_json}" importedTraces 0 path
       "\"\"")
string(JSON broken_json
       SET "${broken_json}" importedTraces 0 format
       "\"wlf\"")
file(WRITE "${broken_project}" "${broken_json}")
file(SHA256 "${broken_project}" broken_sha_before)

string(JSON bad_format_json
       SET "${source_json}" importedTraces 0 path
       "\"${trace_path}\"")
string(JSON bad_format_json
       SET "${bad_format_json}" importedTraces 0 format
       "\"wlf\"")
file(WRITE
    "${bad_format_project}" "${bad_format_json}")

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
string(JSON reference_validation
       GET "${capabilities_json}" features
           traceReferenceValidation)
string(JSON reference_repair
       GET "${capabilities_json}" features
           traceReferenceRepair)
string(JSON operation_count
       GET "${capabilities_json}" operationCount)
string(JSON operation_length
       LENGTH "${capabilities_json}" operations)
set(has_reference_repair FALSE)
math(EXPR operation_last
     "${operation_length} - 1")
foreach(operation_index RANGE 0 ${operation_last})
    string(JSON operation_name
           GET "${capabilities_json}" operations
               ${operation_index} name)
    if(operation_name STREQUAL
       "repair-trace-reference")
        set(has_reference_repair TRUE)
    endif()
endforeach()
if(NOT reference_validation
   OR NOT reference_repair
   OR NOT operation_count EQUAL 33
   OR NOT operation_length EQUAL 33
   OR NOT has_reference_repair)
    message(FATAL_ERROR
        "capabilities omit Imported Trace reference repair")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" validate
            "${broken_project}"
    RESULT_VARIABLE validation_code
    OUTPUT_VARIABLE validation_json
    ERROR_VARIABLE validation_error
)
if(NOT validation_code EQUAL 4)
    message(FATAL_ERROR
        "Invalid Trace reference did not reject validate: ${validation_error}")
endif()
string(JSON reference_issue_count
       GET "${validation_json}"
           traceReferenceIssueCount)
string(JSON repairable_issue_count
       GET "${validation_json}"
           repairableIssueCount)
string(JSON repair_target_count
       GET "${validation_json}"
           repairTargetCount)
string(JSON affected_trace_count
       GET "${validation_json}"
           traceReferenceSummary
           affectedTraceCount)
string(JSON empty_path_count
       GET "${validation_json}"
           traceReferenceSummary
           emptyPathCount)
string(JSON unsupported_format_count
       GET "${validation_json}"
           traceReferenceSummary
           unsupportedFormatCount)
string(JSON issue_count
       LENGTH "${validation_json}" issues)
set(trace_ref "")
math(EXPR issue_last "${issue_count} - 1")
foreach(issue_index RANGE 0 ${issue_last})
    string(JSON issue_code
           GET "${validation_json}" issues
               ${issue_index} code)
    if(issue_code STREQUAL
       "trace-reference-invalid")
        string(JSON trace_ref
               GET "${validation_json}" issues
                   ${issue_index} traceRef)
        string(JSON issue_kind
               GET "${validation_json}" issues
                   ${issue_index} objectKind)
        string(JSON issue_path
               GET "${validation_json}" issues
                   ${issue_index} path)
        string(JSON issue_paths_length
               LENGTH "${validation_json}" issues
                   ${issue_index} paths)
        string(JSON first_problem
               GET "${validation_json}" issues
                   ${issue_index} problems 0)
        string(JSON second_problem
               GET "${validation_json}" issues
                   ${issue_index} problems 1)
        string(JSON repair_operation
               GET "${validation_json}" issues
                   ${issue_index}
                   repairOperations 0)
        string(JSON path_present
               GET "${validation_json}" issues
                   ${issue_index}
                   traceReferenceContext
                   pathPresent)
        string(JSON format_supported
               GET "${validation_json}" issues
                   ${issue_index}
                   traceReferenceContext
                   formatSupported)
        string(JSON filesystem_verification
               GET "${validation_json}" issues
                   ${issue_index}
                   traceReferenceContext
                   filesystemVerification)
    endif()
endforeach()
if(NOT reference_issue_count EQUAL 1
   OR repairable_issue_count LESS 1
   OR repair_target_count LESS 1
   OR NOT affected_trace_count EQUAL 1
   OR NOT empty_path_count EQUAL 1
   OR NOT unsupported_format_count EQUAL 1
   OR trace_ref STREQUAL ""
   OR NOT issue_kind STREQUAL
          "imported-trace-reference"
   OR NOT issue_path STREQUAL
          "importedTraces[0].path"
   OR NOT issue_paths_length EQUAL 2
   OR NOT first_problem STREQUAL "empty-path"
   OR NOT second_problem STREQUAL
          "unsupported-format"
   OR NOT repair_operation STREQUAL
          "repair-trace-reference"
   OR path_present
   OR format_supported
   OR NOT filesystem_verification STREQUAL
          "not-performed")
    message(FATAL_ERROR
        "validate returned incomplete Trace reference diagnostics")
endif()

execute_process(
    COMMAND "${WAVE_COMPARE}"
            "${broken_project}"
            "${broken_compare_dir}"
    RESULT_VARIABLE broken_compare_code
    OUTPUT_VARIABLE broken_compare_output
    ERROR_VARIABLE broken_compare_error
)
if(NOT broken_compare_code EQUAL 2
   OR NOT broken_compare_error MATCHES
          "empty source path")
    message(FATAL_ERROR
        "wave-compare did not explain an empty Trace path")
endif()
execute_process(
    COMMAND "${WAVE_COMPARE}"
            "${bad_format_project}"
            "${bad_format_compare_dir}"
    RESULT_VARIABLE format_compare_code
    OUTPUT_VARIABLE format_compare_output
    ERROR_VARIABLE format_compare_error
)
if(NOT format_compare_code EQUAL 2
   OR NOT format_compare_error MATCHES
          "unsupported; expected VCD, FST, or CSV")
    message(FATAL_ERROR
        "wave-compare did not explain an unsupported Trace format")
endif()

file(WRITE "${operations_file}"
"{
  \"schema\": \"wave-workbench.operations/v1\",
  \"scenarioId\": \"scenario-handshake\",
  \"operations\": [
    {
      \"op\": \"repair-trace-reference\",
      \"traceRef\": \"${trace_ref}\",
      \"path\": \"${trace_path}\",
      \"format\": \"VCD\"
    }
  ]
}
")
execute_process(
    COMMAND "${WAVE_CLI}" apply
            "${broken_project}"
            "${operations_file}"
            "--output=${dry_output}"
            "--expect-sha256=${broken_sha_before}"
            --dry-run
    RESULT_VARIABLE dry_code
    OUTPUT_VARIABLE dry_json
    ERROR_VARIABLE dry_error
)
if(NOT dry_code EQUAL 0
   OR EXISTS "${dry_output}")
    message(FATAL_ERROR
        "Trace reference dry-run failed or wrote output: ${dry_error}")
endif()
string(JSON dry_sha
       GET "${dry_json}" resultSha256)
string(JSON dry_updated
       GET "${dry_json}" operations 0
           updatedTraceCount)
string(JSON dry_repaired
       GET "${dry_json}" operations 0
           repairedTraceReference)
string(JSON dry_format
       GET "${dry_json}" operations 0 format)
string(JSON dry_changed_property_count
       LENGTH "${dry_json}" operations 0
              changedProperties)
string(JSON dry_source_errors
       GET "${dry_json}" validationGuard
           sourceErrorCount)
string(JSON dry_candidate_errors
       GET "${dry_json}" validationGuard
           candidateErrorCount)
if(NOT dry_updated EQUAL 1
   OR NOT dry_repaired
   OR NOT dry_format STREQUAL "vcd"
   OR NOT dry_changed_property_count EQUAL 2
   OR NOT dry_source_errors EQUAL 1
   OR NOT dry_candidate_errors EQUAL 0)
    message(FATAL_ERROR
        "Trace reference dry-run returned the wrong repair result")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" apply
            "${broken_project}"
            "${operations_file}"
            "--output=${repaired_project}"
            "--expect-sha256=${broken_sha_before}"
    RESULT_VARIABLE apply_code
    OUTPUT_VARIABLE apply_json
    ERROR_VARIABLE apply_error
)
if(NOT apply_code EQUAL 0
   OR NOT EXISTS "${repaired_project}")
    message(FATAL_ERROR
        "Trace reference repair failed: ${apply_error}")
endif()
string(JSON apply_sha
       GET "${apply_json}" resultSha256)
string(JSON repaired_trace_ref
       GET "${apply_json}" operations 0 traceRef)
file(SHA256 "${repaired_project}" repaired_sha)
file(READ "${repaired_project}" repaired_json)
string(JSON repaired_id
       GET "${repaired_json}" importedTraces 0 id)
string(JSON repaired_path
       GET "${repaired_json}" importedTraces 0 path)
string(JSON repaired_format
       GET "${repaired_json}" importedTraces 0 format)
string(JSON repaired_offset
       GET "${repaired_json}" importedTraces 0 offsetTick)
string(JSON repaired_mapping
       GET "${repaired_json}" importedTraces 0
           signalMapping lane-request)
if(NOT apply_sha STREQUAL dry_sha
   OR NOT repaired_sha STREQUAL apply_sha
   OR NOT repaired_id STREQUAL
          "trace-handshake-actual"
   OR NOT repaired_path STREQUAL trace_path
   OR NOT repaired_format STREQUAL "vcd"
   OR NOT repaired_offset STREQUAL "0"
   OR NOT repaired_mapping STREQUAL "tb.dut.req")
    message(FATAL_ERROR
        "Trace reference repair changed unrelated state or produced unstable output")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" validate
            "${repaired_project}"
    RESULT_VARIABLE repaired_validation_code
    OUTPUT_VARIABLE repaired_validation_json
    ERROR_VARIABLE repaired_validation_error
)
if(NOT repaired_validation_code EQUAL 0)
    message(FATAL_ERROR
        "Repaired Trace reference remains invalid: ${repaired_validation_error}")
endif()
string(JSON repaired_issue_count
       GET "${repaired_validation_json}"
           traceReferenceIssueCount)
if(NOT repaired_issue_count EQUAL 0)
    message(FATAL_ERROR
        "Repaired Trace reference still has structural issues")
endif()

execute_process(
    COMMAND "${WAVE_COMPARE}"
            "${repaired_project}"
            "${repaired_compare_dir}"
    RESULT_VARIABLE repaired_compare_code
    OUTPUT_VARIABLE repaired_compare_output
    ERROR_VARIABLE repaired_compare_error
)
if(NOT repaired_compare_code EQUAL 0)
    message(FATAL_ERROR
        "wave-compare rejected the repaired Trace reference: ${repaired_compare_error}")
endif()

string(JSON stale_json
       SET "${broken_json}" importedTraces 0 offsetTick
       "\"1\"")
file(WRITE "${stale_project}" "${stale_json}")
execute_process(
    COMMAND "${WAVE_CLI}" apply
            "${stale_project}"
            "${operations_file}"
            "--output=${rejected_output}"
    RESULT_VARIABLE stale_code
    OUTPUT_VARIABLE stale_json_output
    ERROR_VARIABLE stale_error
)
if(stale_code EQUAL 0
   OR EXISTS "${rejected_output}"
   OR NOT stale_error MATCHES "stale")
    message(FATAL_ERROR
        "Trace reference repair accepted a stale snapshot")
endif()

file(WRITE "${partial_operations_file}"
"{
  \"schema\": \"wave-workbench.operations/v1\",
  \"scenarioId\": \"scenario-handshake\",
  \"operations\": [
    {
      \"op\": \"repair-trace-reference\",
      \"traceRef\": \"${trace_ref}\",
      \"path\": \"${trace_path}\"
    }
  ]
}
")
execute_process(
    COMMAND "${WAVE_CLI}" apply
            "${broken_project}"
            "${partial_operations_file}"
            "--output=${rejected_output}"
    RESULT_VARIABLE partial_code
    OUTPUT_VARIABLE partial_json
    ERROR_VARIABLE partial_error
)
if(partial_code EQUAL 0
   OR EXISTS "${rejected_output}"
   OR NOT partial_error MATCHES
          "remains invalid")
    message(FATAL_ERROR
        "Trace reference repair accepted an incomplete replacement")
endif()

file(WRITE "${healthy_operations_file}"
"{
  \"schema\": \"wave-workbench.operations/v1\",
  \"scenarioId\": \"scenario-handshake\",
  \"operations\": [
    {
      \"op\": \"repair-trace-reference\",
      \"traceRef\": \"${repaired_trace_ref}\",
      \"path\": \"${trace_path}\"
    }
  ]
}
")
execute_process(
    COMMAND "${WAVE_CLI}" apply
            "${repaired_project}"
            "${healthy_operations_file}"
            "--output=${rejected_output}"
    RESULT_VARIABLE healthy_code
    OUTPUT_VARIABLE healthy_json
    ERROR_VARIABLE healthy_error
)
if(healthy_code EQUAL 0
   OR EXISTS "${rejected_output}"
   OR NOT healthy_error MATCHES
          "not reported by validate")
    message(FATAL_ERROR
        "Trace reference repair accepted a healthy reference")
endif()

file(SHA256 "${PROJECT}" source_sha_after)
file(SHA256 "${broken_project}" broken_sha_after)
if(NOT source_sha_after STREQUAL source_sha_before
   OR NOT broken_sha_after STREQUAL broken_sha_before)
    message(FATAL_ERROR
        "Trace reference workflow changed a source project")
endif()

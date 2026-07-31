if(NOT DEFINED WAVE_CLI
   OR NOT DEFINED PROJECT
   OR NOT DEFINED OUTPUT)
    message(FATAL_ERROR "Marker validation CLI contract smoke is missing an input")
endif()

set(broken_project "${OUTPUT}-broken.wave.json")
set(blocked_output "${OUTPUT}-blocked.wave.json")
set(progress_project "${OUTPUT}-progress.wave.json")
set(repaired_project "${OUTPUT}-repaired.wave.json")
set(unrelated_operations "${OUTPUT}-unrelated.operations.json")
set(geometry_operations "${OUTPUT}-geometry.operations.json")
set(name_operations "${OUTPUT}-name.operations.json")
file(REMOVE
    "${broken_project}"
    "${blocked_output}"
    "${progress_project}"
    "${repaired_project}"
    "${unrelated_operations}"
    "${geometry_operations}"
    "${name_operations}")

file(SHA256 "${PROJECT}" source_sha_before)
file(READ "${PROJECT}" source_json)
set(original_marker [=[
                {
                    "id": "marker-transfer",
                    "name": "Transfer",
                    "startTick": "80000",
                    "endTick": "150000",
                    "kind": "phase",
                    "note": "Request/acknowledge window"
                }
]=])
set(broken_markers [=[
                {
                    "id": "marker-transfer",
                    "name": "Transfer",
                    "startTick": "80000",
                    "endTick": "150000",
                    "kind": "phase",
                    "note": "Request/acknowledge window"
                },
                {
                    "id": "marker-duplicate-name",
                    "name": " transfer ",
                    "startTick": "160000",
                    "endTick": "170000",
                    "kind": "note",
                    "note": "Imported duplicate name"
                },
                {
                    "id": "marker-broken",
                    "name": "Broken point",
                    "startTick": "-1",
                    "endTick": "230000",
                    "kind": "point",
                    "note": "Imported invalid geometry"
                }
]=])
string(REPLACE
    "${original_marker}"
    "${broken_markers}"
    broken_json
    "${source_json}")
if(broken_json STREQUAL source_json)
    message(FATAL_ERROR "Marker validation fixture was not damaged")
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
string(JSON marker_validation_capability
       GET "${capabilities_json}" features markerIntegrityValidation)
if(NOT marker_validation_capability)
    message(FATAL_ERROR "capabilities omit Marker integrity validation")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" markers "${broken_project}"
    RESULT_VARIABLE marker_query_code
    OUTPUT_VARIABLE marker_query_json
    ERROR_VARIABLE marker_query_error
)
if(NOT marker_query_code EQUAL 0)
    message(FATAL_ERROR "Broken Marker query failed: ${marker_query_error}")
endif()
string(JSON marker_query_issues
       GET "${marker_query_json}" issueCount)
string(JSON marker_query_valid
       GET "${marker_query_json}" validCount)
string(JSON marker_query_addressable
       GET "${marker_query_json}" addressableCount)
if(NOT marker_query_issues EQUAL 5
   OR NOT marker_query_valid EQUAL 0
   OR NOT marker_query_addressable EQUAL 3)
    message(FATAL_ERROR "Marker query fixture does not expose five integrity issues")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" validate "${broken_project}"
    RESULT_VARIABLE broken_validation_code
    OUTPUT_VARIABLE broken_validation_json
    ERROR_VARIABLE broken_validation_error
)
if(NOT broken_validation_code EQUAL 4)
    message(FATAL_ERROR "Marker integrity damage did not reject validate: ${broken_validation_error}")
endif()
string(JSON broken_valid
       GET "${broken_validation_json}" valid)
string(JSON broken_identity_issues
       GET "${broken_validation_json}" identityIssueCount)
string(JSON broken_marker_issues
       GET "${broken_validation_json}" markerIssueCount)
string(JSON broken_marker_valid
       GET "${broken_validation_json}" markerSummary valid)
string(JSON broken_affected_markers
       GET "${broken_validation_json}" markerSummary affectedMarkerCount)
string(JSON broken_name_issues
       GET "${broken_validation_json}" markerSummary nameIssueCount)
string(JSON broken_geometry_issues
       GET "${broken_validation_json}" markerSummary geometryIssueCount)
string(JSON broken_errors
       GET "${broken_validation_json}" summary errors)
string(JSON first_code
       GET "${broken_validation_json}" issues 0 code)
string(JSON first_kind
       GET "${broken_validation_json}" issues 0 objectKind)
string(JSON first_path
       GET "${broken_validation_json}" issues 0 path)
if(broken_valid
   OR NOT broken_identity_issues EQUAL 0
   OR NOT broken_marker_issues EQUAL 5
   OR broken_marker_valid
   OR NOT broken_affected_markers EQUAL 3
   OR NOT broken_name_issues EQUAL 2
   OR NOT broken_geometry_issues EQUAL 3
   OR NOT broken_errors EQUAL 5
   OR NOT first_code STREQUAL "duplicate-name"
   OR NOT first_kind STREQUAL "marker"
   OR NOT first_path STREQUAL "scenarios[0].markers[0].name")
    message(FATAL_ERROR "validate did not return the Marker integrity contract")
endif()

file(WRITE "${unrelated_operations}"
"{
  \"schema\": \"wave-workbench.operations/v1\",
  \"scenarioId\": \"scenario-handshake\",
  \"operations\": [
    {
      \"op\": \"rename-lane\",
      \"laneId\": \"lane-reset\",
      \"name\": \"reset_marker_guard_probe\"
    }
  ]
}
")
execute_process(
    COMMAND "${WAVE_CLI}" apply "${broken_project}" "${unrelated_operations}"
            "--output=${blocked_output}"
    RESULT_VARIABLE blocked_code
    OUTPUT_VARIABLE blocked_stdout
    ERROR_VARIABLE blocked_json
)
string(STRIP "${blocked_stdout}" blocked_stdout)
if(NOT blocked_code EQUAL 4
   OR NOT blocked_stdout STREQUAL ""
   OR EXISTS "${blocked_output}")
    message(FATAL_ERROR "Unrelated edit bypassed Marker integrity validation")
endif()
string(JSON blocked_reason
       GET "${blocked_json}" validationGuard reason)
string(JSON blocked_source_errors
       GET "${blocked_json}" validationGuard sourceErrorCount)
string(JSON blocked_candidate_errors
       GET "${blocked_json}" validationGuard candidateErrorCount)
string(JSON blocked_written GET "${blocked_json}" written)
if(NOT blocked_reason STREQUAL "invalid-candidate"
   OR NOT blocked_source_errors EQUAL 5
   OR NOT blocked_candidate_errors EQUAL 5
   OR blocked_written)
    message(FATAL_ERROR "Marker integrity rejection omitted guard evidence")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" markers "${broken_project}"
            --match=marker-broken --exact
    RESULT_VARIABLE geometry_query_code
    OUTPUT_VARIABLE geometry_query_json
    ERROR_VARIABLE geometry_query_error
)
if(NOT geometry_query_code EQUAL 0)
    message(FATAL_ERROR "Broken geometry query failed: ${geometry_query_error}")
endif()
string(JSON geometry_ref
       GET "${geometry_query_json}" markers 0 markerRef)
file(WRITE "${geometry_operations}"
"{
  \"schema\": \"wave-workbench.operations/v1\",
  \"scenarioId\": \"scenario-handshake\",
  \"operations\": [
    {
      \"op\": \"update-marker\",
      \"markerRef\": \"${geometry_ref}\",
      \"name\": \"Recovered point\",
      \"at\": \"120 ns\"
    }
  ]
}
")
execute_process(
    COMMAND "${WAVE_CLI}" apply "${broken_project}" "${geometry_operations}"
            "--output=${progress_project}" --dry-run
    RESULT_VARIABLE geometry_dry_code
    OUTPUT_VARIABLE geometry_dry_json
    ERROR_VARIABLE geometry_dry_error
)
if(NOT geometry_dry_code EQUAL 0 OR EXISTS "${progress_project}")
    message(FATAL_ERROR "Marker geometry dry-run failed or wrote output: ${geometry_dry_error}")
endif()
string(JSON geometry_dry_sha GET "${geometry_dry_json}" resultSha256)
execute_process(
    COMMAND "${WAVE_CLI}" apply "${broken_project}" "${geometry_operations}"
            "--output=${progress_project}"
    RESULT_VARIABLE geometry_code
    OUTPUT_VARIABLE geometry_json
    ERROR_VARIABLE geometry_error
)
if(NOT geometry_code EQUAL 0 OR NOT EXISTS "${progress_project}")
    message(FATAL_ERROR "Marker geometry repair failed: ${geometry_error}")
endif()
string(JSON geometry_sha GET "${geometry_json}" resultSha256)
string(JSON geometry_reason
       GET "${geometry_json}" validationGuard reason)
string(JSON geometry_source_errors
       GET "${geometry_json}" validationGuard sourceErrorCount)
string(JSON geometry_candidate_errors
       GET "${geometry_json}" validationGuard candidateErrorCount)
string(JSON geometry_new_errors
       GET "${geometry_json}" validationGuard newErrorCount)
if(NOT geometry_sha STREQUAL geometry_dry_sha
   OR NOT geometry_reason STREQUAL "progressive-repair"
   OR NOT geometry_source_errors EQUAL 5
   OR NOT geometry_candidate_errors EQUAL 2
   OR NOT geometry_new_errors EQUAL 0)
    message(FATAL_ERROR "Marker geometry repair did not report 5-to-2 progress")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" markers "${progress_project}"
            --match=marker-duplicate-name --exact
    RESULT_VARIABLE name_query_code
    OUTPUT_VARIABLE name_query_json
    ERROR_VARIABLE name_query_error
)
if(NOT name_query_code EQUAL 0)
    message(FATAL_ERROR "Duplicate name query failed: ${name_query_error}")
endif()
string(JSON name_ref
       GET "${name_query_json}" markers 0 markerRef)
file(WRITE "${name_operations}"
"{
  \"schema\": \"wave-workbench.operations/v1\",
  \"scenarioId\": \"scenario-handshake\",
  \"operations\": [
    {
      \"op\": \"update-marker\",
      \"markerRef\": \"${name_ref}\",
      \"name\": \"Transfer imported\"
    }
  ]
}
")
execute_process(
    COMMAND "${WAVE_CLI}" apply "${progress_project}" "${name_operations}"
            "--output=${repaired_project}" --dry-run
    RESULT_VARIABLE name_dry_code
    OUTPUT_VARIABLE name_dry_json
    ERROR_VARIABLE name_dry_error
)
if(NOT name_dry_code EQUAL 0 OR EXISTS "${repaired_project}")
    message(FATAL_ERROR "Marker name dry-run failed or wrote output: ${name_dry_error}")
endif()
string(JSON name_dry_sha GET "${name_dry_json}" resultSha256)
execute_process(
    COMMAND "${WAVE_CLI}" apply "${progress_project}" "${name_operations}"
            "--output=${repaired_project}"
    RESULT_VARIABLE name_code
    OUTPUT_VARIABLE name_json
    ERROR_VARIABLE name_error
)
if(NOT name_code EQUAL 0 OR NOT EXISTS "${repaired_project}")
    message(FATAL_ERROR "Marker name repair failed: ${name_error}")
endif()
string(JSON name_sha GET "${name_json}" resultSha256)
string(JSON name_reason
       GET "${name_json}" validationGuard reason)
string(JSON name_source_errors
       GET "${name_json}" validationGuard sourceErrorCount)
string(JSON name_candidate_errors
       GET "${name_json}" validationGuard candidateErrorCount)
if(NOT name_sha STREQUAL name_dry_sha
   OR NOT name_reason STREQUAL "valid-candidate"
   OR NOT name_source_errors EQUAL 2
   OR NOT name_candidate_errors EQUAL 0)
    message(FATAL_ERROR "Marker name repair did not produce a valid candidate")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" validate "${repaired_project}"
    RESULT_VARIABLE repaired_validation_code
    OUTPUT_VARIABLE repaired_validation_json
    ERROR_VARIABLE repaired_validation_error
)
if(NOT repaired_validation_code EQUAL 0)
    message(FATAL_ERROR "Repaired Marker project did not validate: ${repaired_validation_error}")
endif()
string(JSON repaired_valid
       GET "${repaired_validation_json}" valid)
string(JSON repaired_marker_issues
       GET "${repaired_validation_json}" markerIssueCount)
string(JSON repaired_errors
       GET "${repaired_validation_json}" summary errors)
if(NOT repaired_valid
   OR NOT repaired_marker_issues EQUAL 0
   OR NOT repaired_errors EQUAL 0)
    message(FATAL_ERROR "Marker integrity repair left validation errors")
endif()

file(SHA256 "${PROJECT}" source_sha_after)
file(SHA256 "${broken_project}" broken_sha_after)
if(NOT source_sha_after STREQUAL source_sha_before
   OR NOT broken_sha_after STREQUAL broken_sha_before)
    message(FATAL_ERROR "Marker validation workflow changed a source project")
endif()

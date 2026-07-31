if(NOT DEFINED WAVE_CLI
   OR NOT DEFINED PROJECT
   OR NOT DEFINED OUTPUT)
    message(FATAL_ERROR "Post-validation guard CLI contract smoke is missing an input")
endif()

set(broken_project "${OUTPUT}-broken.wave.json")
set(blocked_output "${OUTPUT}-blocked.wave.json")
set(progress_project "${OUTPUT}-progress.wave.json")
set(repaired_project "${OUTPUT}-repaired.wave.json")
set(unrelated_operations "${OUTPUT}-unrelated.operations.json")
set(relation_operations "${OUTPUT}-relation.operations.json")
set(marker_operations "${OUTPUT}-marker.operations.json")
file(REMOVE
    "${broken_project}"
    "${blocked_output}"
    "${progress_project}"
    "${repaired_project}"
    "${unrelated_operations}"
    "${relation_operations}"
    "${marker_operations}")

file(SHA256 "${PROJECT}" source_sha_before)
file(READ "${PROJECT}" source_json)
set(original_relation [=[
                {
                    "id": "relation-req-ack",
                    "sourceEventId": "event-req-high",
                    "targetEventId": "event-ack-high",
                    "minimumDelayTick": "10000",
                    "maximumDelayTick": "40000",
                    "clockDomainId": "clock-main",
                    "condition": "",
                    "severity": "error",
                    "description": "ack must rise within 1..4 cycles after req"
                }
]=])
set(duplicate_relations [=[
                {
                    "id": "relation-req-ack",
                    "sourceEventId": "event-req-high",
                    "targetEventId": "event-ack-high",
                    "minimumDelayTick": "10000",
                    "maximumDelayTick": "40000",
                    "clockDomainId": "clock-main",
                    "condition": "",
                    "severity": "error",
                    "description": "ack must rise within 1..4 cycles after req"
                },
                {
                    "id": "relation-req-ack",
                    "sourceEventId": "event-req-high",
                    "targetEventId": "event-ack-high",
                    "minimumDelayTick": "10000",
                    "maximumDelayTick": "40000",
                    "clockDomainId": "clock-main",
                    "condition": "",
                    "severity": "warning",
                    "description": "Imported duplicate Relation"
                }
]=])
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
set(duplicate_markers [=[
                {
                    "id": "marker-transfer",
                    "name": "Transfer",
                    "startTick": "80000",
                    "endTick": "150000",
                    "kind": "phase",
                    "note": "Request/acknowledge window"
                },
                {
                    "id": "marker-transfer",
                    "name": "Transfer imported",
                    "startTick": "160000",
                    "endTick": "170000",
                    "kind": "note",
                    "note": "Imported duplicate Marker"
                }
]=])
string(REPLACE
    "${original_relation}"
    "${duplicate_relations}"
    broken_json
    "${source_json}")
string(REPLACE
    "${original_marker}"
    "${duplicate_markers}"
    broken_json
    "${broken_json}")
if(broken_json STREQUAL source_json)
    message(FATAL_ERROR "Post-validation guard fixture was not damaged")
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
string(JSON guard_capability
       GET "${capabilities_json}" features nonRegressiveApplyValidation)
if(NOT guard_capability)
    message(FATAL_ERROR "capabilities omit non-regressive apply validation")
endif()

file(WRITE "${unrelated_operations}"
"{
  \"schema\": \"wave-workbench.operations/v1\",
  \"scenarioId\": \"scenario-handshake\",
  \"operations\": [
    {
      \"op\": \"rename-lane\",
      \"laneId\": \"lane-reset\",
      \"name\": \"reset_guard_probe\"
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
    message(FATAL_ERROR "Non-improving invalid candidate was not atomically rejected")
endif()
string(JSON blocked_ok GET "${blocked_json}" ok)
string(JSON blocked_error_code GET "${blocked_json}" error code)
string(JSON blocked_candidate_changed GET "${blocked_json}" candidateChanged)
string(JSON blocked_valid GET "${blocked_json}" validation valid)
string(JSON blocked_identity_issues
       GET "${blocked_json}" validation identityIssueCount)
string(JSON blocked_accepted
       GET "${blocked_json}" validationGuard accepted)
string(JSON blocked_reason
       GET "${blocked_json}" validationGuard reason)
string(JSON blocked_source_errors
       GET "${blocked_json}" validationGuard sourceErrorCount)
string(JSON blocked_candidate_errors
       GET "${blocked_json}" validationGuard candidateErrorCount)
string(JSON blocked_new_errors
       GET "${blocked_json}" validationGuard newErrorCount)
string(JSON blocked_resolved_errors
       GET "${blocked_json}" validationGuard resolvedErrorCount)
string(JSON blocked_no_new
       GET "${blocked_json}" validationGuard noNewErrors)
string(JSON blocked_repair_reported
       GET "${blocked_json}" validationGuard repairOperationReported)
string(JSON blocked_written GET "${blocked_json}" written)
if(blocked_ok
   OR NOT blocked_error_code STREQUAL "post-validation-failed"
   OR NOT blocked_candidate_changed
   OR blocked_valid
   OR NOT blocked_identity_issues EQUAL 4
   OR blocked_accepted
   OR NOT blocked_reason STREQUAL "invalid-candidate"
   OR NOT blocked_source_errors EQUAL 4
   OR NOT blocked_candidate_errors EQUAL 4
   OR NOT blocked_new_errors EQUAL 0
   OR NOT blocked_resolved_errors EQUAL 0
   OR NOT blocked_no_new
   OR blocked_repair_reported
   OR blocked_written)
    message(FATAL_ERROR "Rejected apply omitted its validation guard evidence")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" relations "${broken_project}"
            --match=relation-req-ack --exact
    RESULT_VARIABLE relation_query_code
    OUTPUT_VARIABLE relation_query_json
    ERROR_VARIABLE relation_query_error
)
if(NOT relation_query_code EQUAL 0)
    message(FATAL_ERROR "Relation recovery query failed: ${relation_query_error}")
endif()
string(JSON relation_ref
       GET "${relation_query_json}" relations 1 relationRef)
file(WRITE "${relation_operations}"
"{
  \"schema\": \"wave-workbench.operations/v1\",
  \"scenarioId\": \"scenario-handshake\",
  \"operations\": [
    {
      \"op\": \"update-relation\",
      \"relationRef\": \"${relation_ref}\",
      \"newId\": \"relation-req-ack-follow-up\"
    }
  ]
}
")
execute_process(
    COMMAND "${WAVE_CLI}" apply "${broken_project}" "${relation_operations}"
            "--output=${progress_project}"
    RESULT_VARIABLE progress_code
    OUTPUT_VARIABLE progress_json
    ERROR_VARIABLE progress_error
)
if(NOT progress_code EQUAL 0 OR NOT EXISTS "${progress_project}")
    message(FATAL_ERROR "Progressive Relation repair failed: ${progress_error}")
endif()
string(JSON progress_ok GET "${progress_json}" ok)
string(JSON progress_valid GET "${progress_json}" validation valid)
string(JSON progress_identity_issues
       GET "${progress_json}" validation identityIssueCount)
string(JSON progress_accepted
       GET "${progress_json}" validationGuard accepted)
string(JSON progress_candidate_valid
       GET "${progress_json}" validationGuard candidateValid)
string(JSON progressive_repair
       GET "${progress_json}" validationGuard progressiveRepair)
string(JSON progress_reason
       GET "${progress_json}" validationGuard reason)
string(JSON progress_source_errors
       GET "${progress_json}" validationGuard sourceErrorCount)
string(JSON progress_candidate_errors
       GET "${progress_json}" validationGuard candidateErrorCount)
string(JSON progress_new_errors
       GET "${progress_json}" validationGuard newErrorCount)
string(JSON progress_resolved_errors
       GET "${progress_json}" validationGuard resolvedErrorCount)
if(NOT progress_ok
   OR progress_valid
   OR NOT progress_identity_issues EQUAL 2
   OR NOT progress_accepted
   OR progress_candidate_valid
   OR NOT progressive_repair
   OR NOT progress_reason STREQUAL "progressive-repair"
   OR NOT progress_source_errors EQUAL 4
   OR NOT progress_candidate_errors EQUAL 2
   OR NOT progress_new_errors EQUAL 0
   OR NOT progress_resolved_errors EQUAL 2)
    message(FATAL_ERROR "Progressive repair did not report a non-regressive validation transition")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" validate "${progress_project}"
    RESULT_VARIABLE progress_validation_code
    OUTPUT_VARIABLE progress_validation_json
    ERROR_VARIABLE progress_validation_error
)
if(NOT progress_validation_code EQUAL 4)
    message(FATAL_ERROR "Progress project unexpectedly passed final validation")
endif()
string(JSON remaining_kind
       GET "${progress_validation_json}" issues 0 objectKind)
if(NOT remaining_kind STREQUAL "marker")
    message(FATAL_ERROR "Progress project did not isolate the remaining Marker identity issue")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" markers "${progress_project}"
            --match=marker-transfer --exact
    RESULT_VARIABLE marker_query_code
    OUTPUT_VARIABLE marker_query_json
    ERROR_VARIABLE marker_query_error
)
if(NOT marker_query_code EQUAL 0)
    message(FATAL_ERROR "Marker recovery query failed: ${marker_query_error}")
endif()
string(JSON marker_ref
       GET "${marker_query_json}" markers 1 markerRef)
file(WRITE "${marker_operations}"
"{
  \"schema\": \"wave-workbench.operations/v1\",
  \"scenarioId\": \"scenario-handshake\",
  \"operations\": [
    {
      \"op\": \"update-marker\",
      \"markerRef\": \"${marker_ref}\",
      \"newId\": \"marker-transfer-imported\"
    }
  ]
}
")
execute_process(
    COMMAND "${WAVE_CLI}" apply "${progress_project}" "${marker_operations}"
            "--output=${repaired_project}"
    RESULT_VARIABLE repair_code
    OUTPUT_VARIABLE repair_json
    ERROR_VARIABLE repair_error
)
if(NOT repair_code EQUAL 0 OR NOT EXISTS "${repaired_project}")
    message(FATAL_ERROR "Final Marker repair failed: ${repair_error}")
endif()
string(JSON repaired_valid GET "${repair_json}" validation valid)
string(JSON repaired_reason
       GET "${repair_json}" validationGuard reason)
string(JSON repaired_source_errors
       GET "${repair_json}" validationGuard sourceErrorCount)
string(JSON repaired_candidate_errors
       GET "${repair_json}" validationGuard candidateErrorCount)
string(JSON repaired_new_errors
       GET "${repair_json}" validationGuard newErrorCount)
string(JSON repaired_resolved_errors
       GET "${repair_json}" validationGuard resolvedErrorCount)
if(NOT repaired_valid
   OR NOT repaired_reason STREQUAL "valid-candidate"
   OR NOT repaired_source_errors EQUAL 2
   OR NOT repaired_candidate_errors EQUAL 0
   OR NOT repaired_new_errors EQUAL 0
   OR NOT repaired_resolved_errors EQUAL 2)
    message(FATAL_ERROR "Final repair did not produce a fully valid candidate")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" validate "${repaired_project}"
    RESULT_VARIABLE repaired_validation_code
    OUTPUT_VARIABLE repaired_validation_json
    ERROR_VARIABLE repaired_validation_error
)
if(NOT repaired_validation_code EQUAL 0)
    message(FATAL_ERROR "Fully repaired project did not validate: ${repaired_validation_error}")
endif()
string(JSON repaired_accepted GET "${repaired_validation_json}" accepted)
if(NOT repaired_accepted)
    message(FATAL_ERROR "Fully repaired project was not accepted")
endif()

file(SHA256 "${PROJECT}" source_sha_after)
file(SHA256 "${broken_project}" broken_sha_after)
if(NOT source_sha_after STREQUAL source_sha_before
   OR NOT broken_sha_after STREQUAL broken_sha_before)
    message(FATAL_ERROR "Post-validation guard workflow changed a source project")
endif()

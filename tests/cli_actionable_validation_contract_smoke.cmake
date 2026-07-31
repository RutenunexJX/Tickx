if(NOT DEFINED WAVE_CLI
   OR NOT DEFINED PROJECT
   OR NOT DEFINED OUTPUT)
    message(FATAL_ERROR "Actionable validation CLI contract smoke is missing an input")
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
set(broken_relations [=[
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
                    "severity": "error",
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
set(broken_marker [=[
                {
                    "id": "marker-actionable",
                    "name": "Imported point",
                    "startTick": "-1",
                    "endTick": "230000",
                    "kind": "point",
                    "note": "Imported invalid Marker"
                }
]=])
string(REPLACE
    "${original_relation}"
    "${broken_relations}"
    broken_json
    "${source_json}")
string(REPLACE
    "${original_marker}"
    "${broken_marker}"
    broken_json
    "${broken_json}")
if(broken_json STREQUAL source_json)
    message(FATAL_ERROR "Actionable validation fixture was not damaged")
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
string(JSON actionable_capability
       GET "${capabilities_json}" features actionableValidationReferences)
if(NOT actionable_capability)
    message(FATAL_ERROR "capabilities omit actionable validation references")
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
string(JSON baseline_repairable
       GET "${baseline_json}" repairableIssueCount)
string(JSON baseline_repair_targets
       GET "${baseline_json}" repairTargetCount)

execute_process(
    COMMAND "${WAVE_CLI}" validate "${broken_project}"
    RESULT_VARIABLE validation_code
    OUTPUT_VARIABLE validation_json
    ERROR_VARIABLE validation_error
)
if(NOT validation_code EQUAL 4)
    message(FATAL_ERROR "Damaged project did not reject validate: ${validation_error}")
endif()
string(JSON identity_issues
       GET "${validation_json}" identityIssueCount)
string(JSON marker_issues
       GET "${validation_json}" markerIssueCount)
string(JSON repairable_issues
       GET "${validation_json}" repairableIssueCount)
string(JSON repair_targets
       GET "${validation_json}" repairTargetCount)
string(JSON validation_errors
       GET "${validation_json}" summary errors)
math(EXPR expected_repairable "${baseline_repairable} + 5")
math(EXPR expected_repair_targets "${baseline_repair_targets} + 3")
if(NOT identity_issues EQUAL 2
   OR NOT marker_issues EQUAL 3
   OR NOT repairable_issues EQUAL expected_repairable
   OR NOT repair_targets EQUAL expected_repair_targets
   OR NOT validation_errors EQUAL 5)
    message(FATAL_ERROR "validate did not expose the actionable repair counts")
endif()

set(first_relation_ref "")
set(second_relation_ref "")
set(marker_ref "")
string(JSON issue_count LENGTH "${validation_json}" issues)
math(EXPR last_issue "${issue_count} - 1")
foreach(issue_index RANGE 0 ${last_issue})
    string(JSON issue_path
           ERROR_VARIABLE issue_path_error
           GET "${validation_json}" issues ${issue_index} path)
    if(issue_path STREQUAL "scenarios[0].relations[0].id")
        string(JSON first_relation_ref
               GET "${validation_json}" issues ${issue_index} relationRef)
        string(JSON relation_update_operation
               GET "${validation_json}" issues ${issue_index}
                   repairOperations 0)
        string(JSON relation_delete_operation
               GET "${validation_json}" issues ${issue_index}
                   repairOperations 1)
        if(NOT relation_update_operation STREQUAL "update-relation"
           OR NOT relation_delete_operation STREQUAL "delete-relation")
            message(FATAL_ERROR "Relation validation issue omitted repair operations")
        endif()
    elseif(issue_path STREQUAL "scenarios[0].relations[1].id")
        string(JSON second_relation_ref
               GET "${validation_json}" issues ${issue_index} relationRef)
    elseif(issue_path STREQUAL "scenarios[0].markers[0].startTick")
        string(JSON marker_ref
               GET "${validation_json}" issues ${issue_index} markerRef)
        string(JSON marker_update_operation
               GET "${validation_json}" issues ${issue_index}
                   repairOperations 0)
        string(JSON marker_delete_operation
               GET "${validation_json}" issues ${issue_index}
                   repairOperations 1)
        if(NOT marker_update_operation STREQUAL "update-marker"
           OR NOT marker_delete_operation STREQUAL "delete-marker")
            message(FATAL_ERROR "Marker validation issue omitted repair operations")
        endif()
    endif()
endforeach()
if(first_relation_ref STREQUAL ""
   OR second_relation_ref STREQUAL ""
   OR marker_ref STREQUAL ""
   OR first_relation_ref STREQUAL second_relation_ref)
    message(FATAL_ERROR "validate did not return distinct direct repair references")
endif()

file(WRITE "${operations_file}"
"{
  \"schema\": \"wave-workbench.operations/v1\",
  \"scenarioId\": \"scenario-handshake\",
  \"operations\": [
    {
      \"op\": \"update-relation\",
      \"relationRef\": \"${second_relation_ref}\",
      \"newId\": \"relation-actionable-recovered\"
    },
    {
      \"op\": \"update-marker\",
      \"markerRef\": \"${marker_ref}\",
      \"at\": \"120 ns\"
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
    message(FATAL_ERROR "Direct-reference dry-run failed or wrote output: ${dry_error}")
endif()
string(JSON dry_sha GET "${dry_json}" resultSha256)
string(JSON dry_source_errors
       GET "${dry_json}" validationGuard sourceErrorCount)
string(JSON dry_candidate_errors
       GET "${dry_json}" validationGuard candidateErrorCount)
string(JSON dry_reason
       GET "${dry_json}" validationGuard reason)
if(NOT dry_source_errors EQUAL 5
   OR NOT dry_candidate_errors EQUAL 0
   OR NOT dry_reason STREQUAL "valid-candidate")
    message(FATAL_ERROR "Direct-reference dry-run did not repair all damage")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" apply "${broken_project}" "${operations_file}"
            "--output=${repaired_project}"
    RESULT_VARIABLE apply_code
    OUTPUT_VARIABLE apply_json
    ERROR_VARIABLE apply_error
)
if(NOT apply_code EQUAL 0 OR NOT EXISTS "${repaired_project}")
    message(FATAL_ERROR "Direct-reference apply failed: ${apply_error}")
endif()
string(JSON apply_sha GET "${apply_json}" resultSha256)
if(NOT apply_sha STREQUAL dry_sha)
    message(FATAL_ERROR "Direct-reference dry-run and write SHA differ")
endif()
file(SHA256 "${repaired_project}" repaired_sha)
if(NOT repaired_sha STREQUAL apply_sha)
    message(FATAL_ERROR "Direct-reference result SHA does not match the file")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" validate "${repaired_project}"
    RESULT_VARIABLE final_code
    OUTPUT_VARIABLE final_json
    ERROR_VARIABLE final_error
)
if(NOT final_code EQUAL 0)
    message(FATAL_ERROR "Direct-reference result did not validate: ${final_error}")
endif()
string(JSON final_valid GET "${final_json}" valid)
string(JSON final_identity_issues
       GET "${final_json}" identityIssueCount)
string(JSON final_marker_issues
       GET "${final_json}" markerIssueCount)
string(JSON final_repairable_issues
       GET "${final_json}" repairableIssueCount)
string(JSON final_repair_targets
       GET "${final_json}" repairTargetCount)
if(NOT final_valid
   OR NOT final_identity_issues EQUAL 0
   OR NOT final_marker_issues EQUAL 0
   OR NOT final_repairable_issues EQUAL baseline_repairable
   OR NOT final_repair_targets EQUAL baseline_repair_targets)
    message(FATAL_ERROR "Direct-reference repair left validation damage")
endif()

file(SHA256 "${PROJECT}" source_sha_after)
file(SHA256 "${broken_project}" broken_sha_after)
if(NOT source_sha_after STREQUAL source_sha_before
   OR NOT broken_sha_after STREQUAL broken_sha_before)
    message(FATAL_ERROR "Direct-reference workflow changed a source project")
endif()

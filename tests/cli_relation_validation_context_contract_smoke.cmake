if(NOT DEFINED WAVE_CLI
   OR NOT DEFINED PROJECT
   OR NOT DEFINED OUTPUT)
    message(FATAL_ERROR "Relation validation context CLI contract smoke is missing an input")
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
    "\"targetEventId\": \"event-ack-high\""
    "\"targetEventId\": \"missing-structured-target\""
    broken_json
    "${source_json}")
if(broken_json STREQUAL source_json)
    message(FATAL_ERROR "Relation validation context fixture was not damaged")
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
       GET "${capabilities_json}" features structuredRelationValidation)
if(NOT structured_capability)
    message(FATAL_ERROR "capabilities omit structured Relation validation")
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

execute_process(
    COMMAND "${WAVE_CLI}" validate "${broken_project}"
    RESULT_VARIABLE validation_code
    OUTPUT_VARIABLE validation_json
    ERROR_VARIABLE validation_error
)
if(NOT validation_code EQUAL 4)
    message(FATAL_ERROR "Missing Relation target did not reject validate: ${validation_error}")
endif()

set(relation_issue_index "")
string(JSON issue_count LENGTH "${validation_json}" issues)
math(EXPR last_issue "${issue_count} - 1")
foreach(issue_index RANGE 0 ${last_issue})
    string(JSON issue_code
           GET "${validation_json}" issues ${issue_index} code)
    if(issue_code STREQUAL "missing-target-event")
        set(relation_issue_index "${issue_index}")
        break()
    endif()
endforeach()
if(relation_issue_index STREQUAL "")
    message(FATAL_ERROR "validate omitted the missing Relation target issue")
endif()

string(JSON object_kind
       GET "${validation_json}" issues ${relation_issue_index} objectKind)
string(JSON scenario_index
       GET "${validation_json}" issues ${relation_issue_index} scenarioIndex)
string(JSON object_index
       GET "${validation_json}" issues ${relation_issue_index} objectIndex)
string(JSON issue_path
       GET "${validation_json}" issues ${relation_issue_index} path)
string(JSON first_path
       GET "${validation_json}" issues ${relation_issue_index} paths 0)
string(JSON repair_property
       GET "${validation_json}" issues ${relation_issue_index}
           repairProperties 0)
string(JSON relation_ref
       GET "${validation_json}" issues ${relation_issue_index} relationRef)
string(JSON source_ready
       GET "${validation_json}" issues ${relation_issue_index}
           relationContext source relationEndpoint)
string(JSON source_lane
       GET "${validation_json}" issues ${relation_issue_index}
           relationContext source laneId)
string(JSON source_time
       GET "${validation_json}" issues ${relation_issue_index}
           relationContext source timeTick)
string(JSON source_edge
       GET "${validation_json}" issues ${relation_issue_index}
           relationContext source edge)
string(JSON source_value
       GET "${validation_json}" issues ${relation_issue_index}
           relationContext source value)
string(JSON target_ready
       GET "${validation_json}" issues ${relation_issue_index}
           relationContext target relationEndpoint)
string(JSON target_issue
       GET "${validation_json}" issues ${relation_issue_index}
           relationContext target issue)
string(JSON endpoints_ready
       GET "${validation_json}" issues ${relation_issue_index}
           relationContext endpointsReady)
string(JSON minimum_delay
       GET "${validation_json}" issues ${relation_issue_index}
           relationContext minimumDelayTick)
string(JSON maximum_delay
       GET "${validation_json}" issues ${relation_issue_index}
           relationContext maximumDelayTick)
string(JSON observed_type
       TYPE "${validation_json}" issues ${relation_issue_index}
            relationContext observedDelayTick)
if(NOT object_kind STREQUAL "relation"
   OR NOT scenario_index EQUAL 0
   OR NOT object_index EQUAL 0
   OR NOT issue_path STREQUAL "scenarios[0].relations[0].targetEventId"
   OR NOT first_path STREQUAL issue_path
   OR NOT repair_property STREQUAL "target-endpoint"
   OR relation_ref STREQUAL ""
   OR NOT source_ready
   OR NOT source_lane STREQUAL "lane-request"
   OR NOT source_time EQUAL 80000
   OR NOT source_edge STREQUAL "rising"
   OR NOT source_value STREQUAL "1"
   OR target_ready
   OR NOT target_issue STREQUAL "missing-event"
   OR endpoints_ready
   OR NOT minimum_delay EQUAL 10000
   OR NOT maximum_delay EQUAL 40000
   OR NOT observed_type STREQUAL "NULL")
    message(FATAL_ERROR "validate did not return the structured Relation context")
endif()

file(WRITE "${operations_file}"
"{
  \"schema\": \"wave-workbench.operations/v1\",
  \"scenarioId\": \"scenario-handshake\",
  \"operations\": [
    {
      \"op\": \"update-relation\",
      \"relationRef\": \"${relation_ref}\",
      \"targetLaneId\": \"ack\",
      \"targetAt\": \"110 ns\"
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
    message(FATAL_ERROR "Structured Relation dry-run failed or wrote output: ${dry_error}")
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
    message(FATAL_ERROR "Structured Relation repair did not remove the target error")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" apply "${broken_project}" "${operations_file}"
            "--output=${repaired_project}"
    RESULT_VARIABLE apply_code
    OUTPUT_VARIABLE apply_json
    ERROR_VARIABLE apply_error
)
if(NOT apply_code EQUAL 0 OR NOT EXISTS "${repaired_project}")
    message(FATAL_ERROR "Structured Relation apply failed: ${apply_error}")
endif()
string(JSON apply_sha GET "${apply_json}" resultSha256)
file(SHA256 "${repaired_project}" repaired_sha)
if(NOT apply_sha STREQUAL dry_sha
   OR NOT repaired_sha STREQUAL apply_sha)
    message(FATAL_ERROR "Structured Relation dry-run and write SHA differ")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" validate "${repaired_project}"
    RESULT_VARIABLE final_code
    OUTPUT_VARIABLE final_json
    ERROR_VARIABLE final_error
)
if(NOT final_code EQUAL 0)
    message(FATAL_ERROR "Structured Relation result did not validate: ${final_error}")
endif()
string(JSON final_valid GET "${final_json}" valid)
string(JSON final_errors GET "${final_json}" summary errors)
string(JSON final_repairable GET "${final_json}" repairableIssueCount)
if(NOT final_valid
   OR NOT final_errors EQUAL 0
   OR NOT final_repairable EQUAL baseline_repairable)
    message(FATAL_ERROR "Structured Relation repair left validation damage")
endif()

file(SHA256 "${PROJECT}" source_sha_after)
file(SHA256 "${broken_project}" broken_sha_after)
if(NOT source_sha_after STREQUAL source_sha_before
   OR NOT broken_sha_after STREQUAL broken_sha_before)
    message(FATAL_ERROR "Structured Relation workflow changed a source project")
endif()

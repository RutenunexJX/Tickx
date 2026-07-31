if(NOT DEFINED WAVE_CLI
   OR NOT DEFINED PROJECT
   OR NOT DEFINED OUTPUT)
    message(FATAL_ERROR "Identity validation CLI contract smoke is missing an input")
endif()

set(broken_project "${OUTPUT}-broken.wave.json")
set(repaired_project "${OUTPUT}-repaired.wave.json")
set(repair_operations "${OUTPUT}-repair.operations.json")
file(REMOVE
    "${broken_project}"
    "${repaired_project}"
    "${repair_operations}")

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
                    "description": "Imported duplicate identity"
                }
]=])
string(REPLACE
    "${original_relation}"
    "${duplicate_relations}"
    broken_json
    "${source_json}")
if(broken_json STREQUAL source_json)
    message(FATAL_ERROR "Identity validation fixture did not create a duplicate ID")
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
string(JSON identity_validation_capability
       GET "${capabilities_json}" features structuralIdentityValidation)
if(NOT identity_validation_capability)
    message(FATAL_ERROR "capabilities omit structural identity validation")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" validate "${broken_project}"
            --scenario=scenario-handshake
    RESULT_VARIABLE broken_code
    OUTPUT_VARIABLE broken_validation
    ERROR_VARIABLE broken_error
)
if(NOT broken_code EQUAL 4)
    message(FATAL_ERROR "duplicate stable IDs did not reject validation: ${broken_error}")
endif()
string(JSON broken_ok GET "${broken_validation}" ok)
string(JSON broken_accepted GET "${broken_validation}" accepted)
string(JSON broken_valid GET "${broken_validation}" valid)
string(JSON broken_errors GET "${broken_validation}" summary errors)
string(JSON identity_issue_count
       GET "${broken_validation}" identityIssueCount)
string(JSON semantic_issue_count
       GET "${broken_validation}" semanticIssueCount)
string(JSON identity_valid
       GET "${broken_validation}" identitySummary valid)
string(JSON affected_count
       GET "${broken_validation}" identitySummary affectedObjectCount)
string(JSON missing_count
       GET "${broken_validation}" identitySummary missingStableIdCount)
string(JSON duplicate_count
       GET "${broken_validation}" identitySummary duplicateStableIdCount)
string(JSON first_code GET "${broken_validation}" issues 0 code)
string(JSON first_kind GET "${broken_validation}" issues 0 objectKind)
string(JSON first_path GET "${broken_validation}" issues 0 path)
string(JSON first_id_count GET "${broken_validation}" issues 0 idCount)
string(JSON second_code GET "${broken_validation}" issues 1 code)
if(broken_ok
   OR broken_accepted
   OR broken_valid
   OR NOT broken_errors EQUAL 2
   OR NOT identity_issue_count EQUAL 2
   OR semantic_issue_count LESS 1
   OR identity_valid
   OR NOT affected_count EQUAL 2
   OR NOT missing_count EQUAL 0
   OR NOT duplicate_count EQUAL 2
   OR NOT first_code STREQUAL "duplicate-stable-id"
   OR NOT second_code STREQUAL "duplicate-stable-id"
   OR NOT first_kind STREQUAL "relation"
   OR NOT first_path STREQUAL "scenarios[0].relations[0].id"
   OR NOT first_id_count EQUAL 2)
    message(FATAL_ERROR "validate did not return the structural identity contract")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" relations "${broken_project}"
            --match=relation-req-ack --exact
    RESULT_VARIABLE query_code
    OUTPUT_VARIABLE query_json
    ERROR_VARIABLE query_error
)
if(NOT query_code EQUAL 0)
    message(FATAL_ERROR "duplicate Relation query failed: ${query_error}")
endif()
string(JSON second_ref GET "${query_json}" relations 1 relationRef)
if(second_ref STREQUAL "")
    message(FATAL_ERROR "duplicate Relation has no repair reference")
endif()

file(WRITE "${repair_operations}"
"{
  \"schema\": \"wave-workbench.operations/v1\",
  \"scenarioId\": \"scenario-handshake\",
  \"operations\": [
    {
      \"op\": \"update-relation\",
      \"relationRef\": \"${second_ref}\",
      \"newId\": \"relation-req-ack-follow-up\"
    }
  ]
}
")
execute_process(
    COMMAND "${WAVE_CLI}" apply "${broken_project}" "${repair_operations}"
            "--output=${repaired_project}" --dry-run
    RESULT_VARIABLE dry_code
    OUTPUT_VARIABLE dry_json
    ERROR_VARIABLE dry_error
)
if(NOT dry_code EQUAL 0 OR EXISTS "${repaired_project}")
    message(FATAL_ERROR "identity repair dry-run failed or wrote output: ${dry_error}")
endif()
string(JSON dry_sha GET "${dry_json}" resultSha256)

execute_process(
    COMMAND "${WAVE_CLI}" apply "${broken_project}" "${repair_operations}"
            "--output=${repaired_project}"
    RESULT_VARIABLE repair_code
    OUTPUT_VARIABLE repair_json
    ERROR_VARIABLE repair_error
)
if(NOT repair_code EQUAL 0 OR NOT EXISTS "${repaired_project}")
    message(FATAL_ERROR "identity repair write failed: ${repair_error}")
endif()
string(JSON repair_sha GET "${repair_json}" resultSha256)
if(NOT repair_sha STREQUAL dry_sha)
    message(FATAL_ERROR "identity repair dry-run and write SHA differ")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" validate "${repaired_project}"
            --scenario=scenario-handshake
    RESULT_VARIABLE repaired_code
    OUTPUT_VARIABLE repaired_validation
    ERROR_VARIABLE repaired_error
)
if(NOT repaired_code EQUAL 0)
    message(FATAL_ERROR "repaired project did not validate: ${repaired_error}")
endif()
string(JSON repaired_accepted GET "${repaired_validation}" accepted)
string(JSON repaired_valid GET "${repaired_validation}" valid)
string(JSON repaired_errors GET "${repaired_validation}" summary errors)
string(JSON repaired_identity_issues
       GET "${repaired_validation}" identityIssueCount)
string(JSON repaired_identity_valid
       GET "${repaired_validation}" identitySummary valid)
if(NOT repaired_accepted
   OR NOT repaired_valid
   OR NOT repaired_errors EQUAL 0
   OR NOT repaired_identity_issues EQUAL 0
   OR NOT repaired_identity_valid)
    message(FATAL_ERROR "identity repair did not clear structural validation")
endif()

file(SHA256 "${PROJECT}" source_sha_after)
file(SHA256 "${broken_project}" broken_sha_after)
if(NOT source_sha_after STREQUAL source_sha_before
   OR NOT broken_sha_after STREQUAL broken_sha_before)
    message(FATAL_ERROR "identity validation or repair changed a source project")
endif()

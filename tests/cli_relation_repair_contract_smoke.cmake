if(NOT DEFINED WAVE_CLI
   OR NOT DEFINED PROJECT
   OR NOT DEFINED OUTPUT)
    message(FATAL_ERROR "Relation repair CLI contract smoke is missing an input")
endif()

set(broken_project "${OUTPUT}-broken.wave.json")
set(repaired_project "${OUTPUT}-repaired.wave.json")
set(repair_operations "${OUTPUT}-repair.operations.json")
set(partial_operations "${OUTPUT}-partial.operations.json")
set(partial_output "${OUTPUT}-partial.wave.json")
file(REMOVE
    "${broken_project}"
    "${repaired_project}"
    "${repair_operations}"
    "${partial_operations}"
    "${partial_output}")

file(SHA256 "${PROJECT}" source_sha_before)
file(READ "${PROJECT}" source_json)
string(REPLACE
    "\"targetEventId\": \"event-ack-high\""
    "\"targetEventId\": \"missing-target-event\""
    broken_json
    "${source_json}")
if(broken_json STREQUAL source_json)
    message(FATAL_ERROR "Relation repair fixture did not replace the target Event ID")
endif()
file(WRITE "${broken_project}" "${broken_json}")
file(SHA256 "${broken_project}" broken_sha_before)

file(WRITE "${repair_operations}" [=[
{
  "schema": "wave-workbench.operations/v1",
  "scenarioId": "scenario-handshake",
  "operations": [
    {
      "op": "update-relation",
      "relationId": "relation-req-ack",
      "targetLaneId": "ack",
      "targetAt": "110 ns",
      "description": "Repaired request acknowledgement"
    }
  ]
}
]=])
file(WRITE "${partial_operations}" [=[
{
  "schema": "wave-workbench.operations/v1",
  "scenarioId": "scenario-handshake",
  "operations": [
    {
      "op": "update-relation",
      "relationId": "relation-req-ack",
      "targetLaneId": "ack"
    }
  ]
}
]=])

execute_process(
    COMMAND "${WAVE_CLI}" capabilities
    RESULT_VARIABLE capabilities_code
    OUTPUT_VARIABLE capabilities_json
    ERROR_VARIABLE capabilities_error
)
if(NOT capabilities_code EQUAL 0)
    message(FATAL_ERROR "capabilities failed: ${capabilities_error}")
endif()
string(JSON repair_capability
       GET "${capabilities_json}" features relationEndpointRepair)
if(NOT repair_capability)
    message(FATAL_ERROR "capabilities omit Relation endpoint repair")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" relations "${broken_project}"
            --match=relation-req-ack --exact
    RESULT_VARIABLE broken_query_code
    OUTPUT_VARIABLE broken_query_json
    ERROR_VARIABLE broken_query_error
)
if(NOT broken_query_code EQUAL 0)
    message(FATAL_ERROR "broken Relation query failed: ${broken_query_error}")
endif()
string(JSON broken_ready_count GET "${broken_query_json}" readyCount)
string(JSON broken_issue_count GET "${broken_query_json}" endpointIssueCount)
string(JSON broken_target_issue
       GET "${broken_query_json}" relations 0 target issue)
if(NOT broken_ready_count EQUAL 0
   OR NOT broken_issue_count EQUAL 1
   OR NOT broken_target_issue STREQUAL "missing-event")
    message(FATAL_ERROR "broken Relation was not diagnosed before repair")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" apply "${broken_project}" "${repair_operations}"
            "--output=${repaired_project}" --dry-run
    RESULT_VARIABLE dry_code
    OUTPUT_VARIABLE dry_json
    ERROR_VARIABLE dry_error
)
if(NOT dry_code EQUAL 0 OR EXISTS "${repaired_project}")
    message(FATAL_ERROR "Relation repair dry-run failed or wrote output: ${dry_error}")
endif()
string(JSON dry_sha GET "${dry_json}" resultSha256)
string(JSON dry_changed GET "${dry_json}" operations 0 changed)
string(JSON repaired_source
       GET "${dry_json}" operations 0 repairedSourceEndpoint)
string(JSON repaired_target
       GET "${dry_json}" operations 0 repairedTargetEndpoint)
string(JSON target_lane GET "${dry_json}" operations 0 targetLaneId)
string(JSON target_tick GET "${dry_json}" operations 0 targetAtTick)
string(JSON validation_errors
       GET "${dry_json}" validation summary errors)
if(NOT dry_changed
   OR repaired_source
   OR NOT repaired_target
   OR NOT target_lane STREQUAL "lane-ack"
   OR NOT target_tick STREQUAL "110000"
   OR NOT validation_errors STREQUAL "0")
    message(FATAL_ERROR "Relation repair dry-run returned the wrong contract")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" apply "${broken_project}" "${repair_operations}"
            "--output=${repaired_project}"
    RESULT_VARIABLE repair_code
    OUTPUT_VARIABLE repair_json
    ERROR_VARIABLE repair_error
)
if(NOT repair_code EQUAL 0 OR NOT EXISTS "${repaired_project}")
    message(FATAL_ERROR "Relation repair write failed: ${repair_error}")
endif()
string(JSON repaired_sha GET "${repair_json}" resultSha256)
if(NOT repaired_sha STREQUAL dry_sha)
    message(FATAL_ERROR "Relation repair dry-run and write SHA differ")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" relations "${repaired_project}"
            --match=relation-req-ack --exact --lane=ack
    RESULT_VARIABLE repaired_query_code
    OUTPUT_VARIABLE repaired_query_json
    ERROR_VARIABLE repaired_query_error
)
if(NOT repaired_query_code EQUAL 0)
    message(FATAL_ERROR "repaired Relation query failed: ${repaired_query_error}")
endif()
string(JSON repaired_ready_count GET "${repaired_query_json}" readyCount)
string(JSON repaired_issue_count
       GET "${repaired_query_json}" endpointIssueCount)
string(JSON endpoints_ready
       GET "${repaired_query_json}" relations 0 endpointsReady)
string(JSON repaired_target_lane
       GET "${repaired_query_json}" relations 0 target laneId)
string(JSON repaired_target_tick
       GET "${repaired_query_json}" relations 0 target timeTick)
if(NOT repaired_ready_count EQUAL 1
   OR NOT repaired_issue_count EQUAL 0
   OR NOT endpoints_ready
   OR NOT repaired_target_lane STREQUAL "lane-ack"
   OR NOT repaired_target_tick STREQUAL "110000")
    message(FATAL_ERROR "repaired Relation was not immediately usable")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" validate "${repaired_project}"
            --scenario=scenario-handshake
    RESULT_VARIABLE validate_code
    OUTPUT_VARIABLE validate_json
    ERROR_VARIABLE validate_error
)
if(NOT validate_code EQUAL 0)
    message(FATAL_ERROR "repaired project validation failed: ${validate_error}")
endif()
string(JSON repaired_validation_errors
       GET "${validate_json}" summary errors)
if(NOT repaired_validation_errors STREQUAL "0")
    message(FATAL_ERROR "repaired Relation left validation errors")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" apply "${broken_project}" "${partial_operations}"
            "--output=${partial_output}"
    RESULT_VARIABLE partial_code
    OUTPUT_VARIABLE partial_stdout
    ERROR_VARIABLE partial_json
)
if(NOT partial_code EQUAL 4
   OR NOT partial_stdout STREQUAL ""
   OR EXISTS "${partial_output}")
    message(FATAL_ERROR "partial Relation repair was accepted or wrote output")
endif()
string(JSON partial_error_code GET "${partial_json}" error code)
string(JSON partial_operation GET "${partial_json}" error operation)
string(JSON partial_message GET "${partial_json}" error message)
string(FIND "${partial_message}" "'targetLaneId'" lane_field_position)
string(FIND "${partial_message}" "'targetAtTick'" tick_field_position)
if(NOT partial_error_code STREQUAL "operation-rejected"
   OR NOT partial_operation EQUAL 0
   OR lane_field_position EQUAL -1
   OR tick_field_position EQUAL -1)
    message(FATAL_ERROR "partial Relation repair returned the wrong failure")
endif()

file(SHA256 "${PROJECT}" source_sha_after)
file(SHA256 "${broken_project}" broken_sha_after)
if(NOT source_sha_after STREQUAL source_sha_before
   OR NOT broken_sha_after STREQUAL broken_sha_before)
    message(FATAL_ERROR "Relation repair changed a source project")
endif()

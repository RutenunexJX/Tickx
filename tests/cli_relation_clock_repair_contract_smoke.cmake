if(NOT DEFINED WAVE_CLI
   OR NOT DEFINED PROJECT
   OR NOT DEFINED OUTPUT)
    message(FATAL_ERROR "Relation clock repair CLI contract smoke is missing an input")
endif()

set(broken_project "${OUTPUT}-broken.wave.json")
set(repaired_project "${OUTPUT}-repaired.wave.json")
set(ambiguous_project "${OUTPUT}-ambiguous.wave.json")
set(dry_output "${OUTPUT}-dry.wave.json")
set(healthy_output "${OUTPUT}-healthy-output.wave.json")
set(ambiguous_output "${OUTPUT}-ambiguous-output.wave.json")
set(operations_file "${OUTPUT}.operations.json")
file(REMOVE
    "${broken_project}"
    "${repaired_project}"
    "${ambiguous_project}"
    "${dry_output}"
    "${healthy_output}"
    "${ambiguous_output}"
    "${operations_file}")

file(SHA256 "${PROJECT}" source_sha_before)
file(READ "${PROJECT}" source_json)
string(REPLACE
    "\"maximumDelayTick\": \"40000\",\n                    \"clockDomainId\": \"clock-main\",\n                    \"condition\": \"\""
    "\"maximumDelayTick\": \"40000\",\n                    \"clockDomainId\": \"clock-missing\",\n                    \"condition\": \"\""
    broken_json
    "${source_json}")
if(broken_json STREQUAL source_json)
    message(FATAL_ERROR "Relation clock repair fixture was not damaged")
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
string(JSON relation_clock_capability
       GET "${capabilities_json}" features relationClockRepair)
string(JSON operation_count
       GET "${capabilities_json}" operationCount)
string(JSON operation_length
       LENGTH "${capabilities_json}" operations)
set(has_repair_relation_clock FALSE)
math(EXPR operation_last "${operation_length} - 1")
foreach(operation_index RANGE 0 ${operation_last})
    string(JSON operation_name
           GET "${capabilities_json}" operations ${operation_index} name)
    if(operation_name STREQUAL "repair-relation-clock")
        set(has_repair_relation_clock TRUE)
    endif()
endforeach()
if(NOT relation_clock_capability
   OR NOT operation_count EQUAL 33
   OR NOT operation_length EQUAL 33
   OR NOT has_repair_relation_clock)
    message(FATAL_ERROR "capabilities omit Relation clock repair")
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
    message(FATAL_ERROR "Missing Relation ClockDomain did not reject validate: ${validation_error}")
endif()
set(clock_issue_index "")
set(mismatch_count 0)
string(JSON issue_count LENGTH "${validation_json}" issues)
math(EXPR last_issue "${issue_count} - 1")
foreach(issue_index RANGE 0 ${last_issue})
    string(JSON issue_code
           GET "${validation_json}" issues ${issue_index} code)
    string(JSON issue_relation
           GET "${validation_json}" issues ${issue_index} relationId)
    if(issue_code STREQUAL "relation-clock-domain-invalid"
       AND issue_relation STREQUAL "relation-req-ack")
        set(clock_issue_index "${issue_index}")
    elseif(issue_code STREQUAL "clock-domain-mismatch"
           AND issue_relation STREQUAL "relation-req-ack")
        math(EXPR mismatch_count "${mismatch_count} + 1")
    endif()
endforeach()
if(clock_issue_index STREQUAL "")
    message(FATAL_ERROR "validate omitted the invalid Relation ClockDomain")
endif()

string(JSON error_count
       GET "${validation_json}" summary errors)
string(JSON repairable_issue_count
       GET "${validation_json}" repairableIssueCount)
string(JSON repair_target_count
       GET "${validation_json}" repairTargetCount)
string(JSON issue_path
       GET "${validation_json}" issues ${clock_issue_index} path)
string(JSON path_count
       LENGTH "${validation_json}" issues ${clock_issue_index} paths)
string(JSON clock_path
       GET "${validation_json}" issues ${clock_issue_index} paths 0)
string(JSON repair_property
       GET "${validation_json}" issues ${clock_issue_index}
           repairProperties 0)
string(JSON repair_count
       LENGTH "${validation_json}" issues ${clock_issue_index}
           repairOperations)
string(JSON repair_operation
       GET "${validation_json}" issues ${clock_issue_index}
           repairOperations 0)
string(JSON object_kind
       GET "${validation_json}" issues ${clock_issue_index} objectKind)
string(JSON relation_id
       GET "${validation_json}" issues ${clock_issue_index} relationId)
string(JSON relation_index
       GET "${validation_json}" issues ${clock_issue_index} relationIndex)
string(JSON clock_id
       GET "${validation_json}" issues ${clock_issue_index}
           relationClockContext clockDomainId)
string(JSON clock_match_count
       GET "${validation_json}" issues ${clock_issue_index}
           relationClockContext clockDomainMatchCount)
string(JSON clock_valid
       GET "${validation_json}" issues ${clock_issue_index}
           relationClockContext clockReferenceValid)
string(JSON source_event_count
       GET "${validation_json}" issues ${clock_issue_index}
           relationClockContext sourceEventIdCount)
string(JSON source_lane_count
       GET "${validation_json}" issues ${clock_issue_index}
           relationClockContext sourceLaneIdCount)
string(JSON source_clock
       GET "${validation_json}" issues ${clock_issue_index}
           relationClockContext sourceEffectiveClockDomainId)
string(JSON source_clock_count
       GET "${validation_json}" issues ${clock_issue_index}
           relationClockContext sourceClockDomainMatchCount)
string(JSON source_ready
       GET "${validation_json}" issues ${clock_issue_index}
           relationClockContext sourceClockContextReady)
string(JSON target_event_count
       GET "${validation_json}" issues ${clock_issue_index}
           relationClockContext targetEventIdCount)
string(JSON target_lane_count
       GET "${validation_json}" issues ${clock_issue_index}
           relationClockContext targetLaneIdCount)
string(JSON target_clock
       GET "${validation_json}" issues ${clock_issue_index}
           relationClockContext targetEffectiveClockDomainId)
string(JSON target_clock_count
       GET "${validation_json}" issues ${clock_issue_index}
           relationClockContext targetClockDomainMatchCount)
string(JSON target_ready
       GET "${validation_json}" issues ${clock_issue_index}
           relationClockContext targetClockContextReady)
string(JSON endpoint_conflict
       GET "${validation_json}" issues ${clock_issue_index}
           relationClockContext endpointClockConflict)
string(JSON repair_action
       GET "${validation_json}" issues ${clock_issue_index}
           relationClockContext clockRepairAction)
string(JSON replacement_clock
       GET "${validation_json}" issues ${clock_issue_index}
           relationClockContext replacementClockDomainId)
string(JSON clock_repairable
       GET "${validation_json}" issues ${clock_issue_index}
           relationClockContext clockRepairable)
if(NOT error_count EQUAL 2
   OR NOT repairable_issue_count EQUAL 4
   OR NOT repair_target_count EQUAL 2
   OR NOT mismatch_count EQUAL 1
   OR NOT issue_path STREQUAL "scenarios[0].relations[0].clockDomainId"
   OR NOT path_count EQUAL 1
   OR NOT clock_path STREQUAL "scenarios[0].relations[0].clockDomainId"
   OR NOT repair_property STREQUAL "relation-clock"
   OR NOT repair_count EQUAL 1
   OR NOT repair_operation STREQUAL "repair-relation-clock"
   OR NOT object_kind STREQUAL "relation"
   OR NOT relation_id STREQUAL "relation-req-ack"
   OR NOT relation_index EQUAL 0
   OR NOT clock_id STREQUAL "clock-missing"
   OR NOT clock_match_count EQUAL 0
   OR clock_valid
   OR NOT source_event_count EQUAL 1
   OR NOT source_lane_count EQUAL 1
   OR NOT source_clock STREQUAL "clock-main"
   OR NOT source_clock_count EQUAL 1
   OR NOT source_ready
   OR NOT target_event_count EQUAL 1
   OR NOT target_lane_count EQUAL 1
   OR NOT target_clock STREQUAL "clock-main"
   OR NOT target_clock_count EQUAL 1
   OR NOT target_ready
   OR endpoint_conflict
   OR NOT repair_action STREQUAL "use-endpoint-clock"
   OR NOT replacement_clock STREQUAL "clock-main"
   OR NOT clock_repairable)
    message(FATAL_ERROR "validate returned the wrong Relation clock repair context")
endif()

file(WRITE "${operations_file}"
"{
  \"schema\": \"wave-workbench.operations/v1\",
  \"scenarioId\": \"scenario-handshake\",
  \"operations\": [
    {
      \"op\": \"repair-relation-clock\",
      \"relationId\": \"${relation_id}\"
    }
  ]
}
")

execute_process(
    COMMAND "${WAVE_CLI}" apply "${PROJECT}" "${operations_file}"
            "--output=${healthy_output}"
    RESULT_VARIABLE healthy_code
    OUTPUT_VARIABLE healthy_stdout
    ERROR_VARIABLE healthy_error
)
if(NOT healthy_code EQUAL 4
   OR NOT healthy_stdout STREQUAL ""
   OR EXISTS "${healthy_output}")
    message(FATAL_ERROR "Healthy Relation clock repair was not atomically rejected")
endif()
string(FIND "${healthy_error}" "not reported by validate" healthy_error_position)
if(healthy_error_position EQUAL -1)
    message(FATAL_ERROR "Healthy Relation clock repair omitted its validation-only cause")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" apply "${broken_project}" "${operations_file}"
            "--output=${dry_output}" --dry-run
    RESULT_VARIABLE dry_code
    OUTPUT_VARIABLE dry_json
    ERROR_VARIABLE dry_error
)
if(NOT dry_code EQUAL 0 OR EXISTS "${dry_output}")
    message(FATAL_ERROR "Relation clock repair dry-run failed or wrote output: ${dry_error}")
endif()
string(JSON dry_sha GET "${dry_json}" resultSha256)
string(JSON source_errors
       GET "${dry_json}" validationGuard sourceErrorCount)
string(JSON candidate_errors
       GET "${dry_json}" validationGuard candidateErrorCount)
string(JSON guard_reason
       GET "${dry_json}" validationGuard reason)
string(JSON updated_relations
       GET "${dry_json}" operations 0 updatedRelationCount)
string(JSON created_events
       GET "${dry_json}" operations 0 createdEventCount)
string(JSON updated_events
       GET "${dry_json}" operations 0 updatedEventCount)
string(JSON removed_events
       GET "${dry_json}" operations 0 removedEventCount)
string(JSON removed_relations
       GET "${dry_json}" operations 0 removedRelationCount)
string(JSON removed_segments
       GET "${dry_json}" operations 0 removedSegmentCount)
string(JSON before_valid
       GET "${dry_json}" operations 0
           beforeRelationClockContext clockReferenceValid)
string(JSON before_action
       GET "${dry_json}" operations 0
           beforeRelationClockContext clockRepairAction)
string(JSON before_replacement
       GET "${dry_json}" operations 0
           beforeRelationClockContext replacementClockDomainId)
string(JSON after_valid
       GET "${dry_json}" operations 0
           relationClockContext clockReferenceValid)
string(JSON after_clock
       GET "${dry_json}" operations 0
           relationClockContext clockDomainId)
string(JSON relation_clock
       GET "${dry_json}" operations 0
           relationContext clockDomainId)
if(NOT source_errors EQUAL 2
   OR NOT candidate_errors EQUAL 0
   OR NOT guard_reason STREQUAL "valid-candidate"
   OR NOT updated_relations EQUAL 1
   OR NOT created_events EQUAL 0
   OR NOT updated_events EQUAL 0
   OR NOT removed_events EQUAL 0
   OR NOT removed_relations EQUAL 0
   OR NOT removed_segments EQUAL 0
   OR before_valid
   OR NOT before_action STREQUAL "use-endpoint-clock"
   OR NOT before_replacement STREQUAL "clock-main"
   OR NOT after_valid
   OR NOT after_clock STREQUAL "clock-main"
   OR NOT relation_clock STREQUAL "clock-main")
    message(FATAL_ERROR "Relation clock repair changed endpoints or waveform")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" apply "${broken_project}" "${operations_file}"
            "--output=${repaired_project}"
    RESULT_VARIABLE apply_code
    OUTPUT_VARIABLE apply_json
    ERROR_VARIABLE apply_error
)
if(NOT apply_code EQUAL 0 OR NOT EXISTS "${repaired_project}")
    message(FATAL_ERROR "Relation clock repair failed: ${apply_error}")
endif()
string(JSON apply_sha GET "${apply_json}" resultSha256)
file(SHA256 "${repaired_project}" repaired_sha)
if(NOT apply_sha STREQUAL dry_sha
   OR NOT repaired_sha STREQUAL apply_sha)
    message(FATAL_ERROR "Relation clock repair dry-run and write SHA differ")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" validate "${repaired_project}"
    RESULT_VARIABLE final_code
    OUTPUT_VARIABLE final_json
    ERROR_VARIABLE final_error
)
if(NOT final_code EQUAL 0)
    message(FATAL_ERROR "Relation clock repair result did not validate: ${final_error}")
endif()
string(JSON final_valid GET "${final_json}" valid)
string(JSON final_errors GET "${final_json}" summary errors)
file(READ "${repaired_project}" repaired_json)
string(JSON repaired_clock
       GET "${repaired_json}" scenarios 0 relations 0 clockDomainId)
if(NOT final_valid
   OR NOT final_errors EQUAL 0
   OR NOT repaired_clock STREQUAL "clock-main")
    message(FATAL_ERROR "Relation clock repair left validation damage")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" relations "${repaired_project}"
            --match=relation-req-ack --exact
    RESULT_VARIABLE relation_code
    OUTPUT_VARIABLE relation_json
    ERROR_VARIABLE relation_error
)
if(NOT relation_code EQUAL 0)
    message(FATAL_ERROR "Repaired Relation query failed: ${relation_error}")
endif()
string(JSON relation_count GET "${relation_json}" matchCount)
string(JSON relation_ready GET "${relation_json}" relations 0 endpointsReady)
string(JSON query_clock GET "${relation_json}" relations 0 clockDomainId)
string(JSON source_tick GET "${relation_json}" relations 0 source timeTick)
string(JSON source_value GET "${relation_json}" relations 0 source value)
string(JSON target_tick GET "${relation_json}" relations 0 target timeTick)
string(JSON target_value GET "${relation_json}" relations 0 target value)
if(NOT relation_count EQUAL 1
   OR NOT relation_ready
   OR NOT query_clock STREQUAL "clock-main"
   OR NOT source_tick EQUAL 80000
   OR NOT source_value STREQUAL "1"
   OR NOT target_tick EQUAL 110000
   OR NOT target_value STREQUAL "1")
    message(FATAL_ERROR "Relation clock repair did not preserve its endpoints")
endif()

string(JSON relation_object
       GET "${broken_json}" scenarios 0 relations 0)
string(JSON ambiguous_json
       SET "${broken_json}" scenarios 0 relations 1
       "${relation_object}")
file(WRITE "${ambiguous_project}" "${ambiguous_json}")
file(SHA256 "${ambiguous_project}" ambiguous_sha_before)
execute_process(
    COMMAND "${WAVE_CLI}" apply "${ambiguous_project}" "${operations_file}"
            "--output=${ambiguous_output}"
    RESULT_VARIABLE ambiguous_code
    OUTPUT_VARIABLE ambiguous_stdout
    ERROR_VARIABLE ambiguous_error
)
if(NOT ambiguous_code EQUAL 4
   OR NOT ambiguous_stdout STREQUAL ""
   OR EXISTS "${ambiguous_output}")
    message(FATAL_ERROR "Ambiguous Relation clock repair was not atomically rejected")
endif()
string(FIND "${ambiguous_error}" "ambiguous" ambiguous_error_position)
if(ambiguous_error_position EQUAL -1)
    message(FATAL_ERROR "Ambiguous Relation clock repair omitted its cause")
endif()
file(SHA256 "${ambiguous_project}" ambiguous_sha_after)
if(NOT ambiguous_sha_after STREQUAL ambiguous_sha_before)
    message(FATAL_ERROR "Ambiguous Relation clock repair changed its source")
endif()

file(SHA256 "${PROJECT}" source_sha_after)
file(SHA256 "${broken_project}" broken_sha_after)
if(NOT source_sha_after STREQUAL source_sha_before
   OR NOT broken_sha_after STREQUAL broken_sha_before)
    message(FATAL_ERROR "Relation clock repair workflow changed a source project")
endif()

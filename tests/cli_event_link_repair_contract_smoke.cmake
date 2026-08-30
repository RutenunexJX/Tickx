if(NOT DEFINED WAVE_CLI
   OR NOT DEFINED PROJECT
   OR NOT DEFINED OUTPUT)
    message(FATAL_ERROR "Event link repair CLI contract smoke is missing an input")
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
    "\"id\": \"event-ack-high\", \"laneId\": \"lane-ack\", \"timeTick\": \"110000\", \"action\": \"expect\", \"value\": \"1\""
    "\"id\": \"event-ack-high\", \"laneId\": \"lane-reset\", \"timeTick\": \"111000\", \"action\": \"expect\", \"value\": \"0\""
    broken_json
    "${source_json}")
if(broken_json STREQUAL source_json)
    message(FATAL_ERROR "Event link repair fixture was not damaged")
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
string(JSON repair_capability
       GET "${capabilities_json}" features eventLinkRepair)
string(JSON operation_count
       GET "${capabilities_json}" operationCount)
string(JSON operation_length
       LENGTH "${capabilities_json}" operations)
set(has_repair_event_link FALSE)
math(EXPR operation_last "${operation_length} - 1")
foreach(operation_index RANGE 0 ${operation_last})
    string(JSON operation_name
           GET "${capabilities_json}" operations ${operation_index} name)
    if(operation_name STREQUAL "repair-event-link")
        set(has_repair_event_link TRUE)
    endif()
endforeach()
if(NOT repair_capability
   OR NOT operation_count EQUAL 38
   OR NOT operation_length EQUAL 38
   OR NOT has_repair_event_link)
    message(FATAL_ERROR "capabilities omit Event link repair")
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
    message(FATAL_ERROR "Broken Event link did not reject validate: ${validation_error}")
endif()
set(event_issue_index "")
string(JSON issue_count LENGTH "${validation_json}" issues)
math(EXPR last_issue "${issue_count} - 1")
foreach(issue_index RANGE 0 ${last_issue})
    string(JSON issue_code
           GET "${validation_json}" issues ${issue_index} code)
    if(issue_code STREQUAL "event-waveform-mismatch")
        set(event_issue_index "${issue_index}")
        break()
    endif()
endforeach()
if(event_issue_index STREQUAL "")
    message(FATAL_ERROR "validate omitted the Event waveform mismatch")
endif()

string(JSON issue_path
       GET "${validation_json}" issues ${event_issue_index} path)
string(JSON path_count
       LENGTH "${validation_json}" issues ${event_issue_index} paths)
string(JSON lane_path
       GET "${validation_json}" issues ${event_issue_index} paths 0)
string(JSON tick_path
       GET "${validation_json}" issues ${event_issue_index} paths 1)
string(JSON value_path
       GET "${validation_json}" issues ${event_issue_index} paths 2)
string(JSON repair_property
       GET "${validation_json}" issues ${event_issue_index}
           repairProperties 0)
string(JSON repair_operation
       GET "${validation_json}" issues ${event_issue_index}
           repairOperations 0)
string(JSON delete_operation
       GET "${validation_json}" issues ${event_issue_index}
           repairOperations 1)
string(JSON event_id
       GET "${validation_json}" issues ${event_issue_index} eventId)
string(JSON lane_id
       GET "${validation_json}" issues ${event_issue_index}
           eventContext laneId)
string(JSON lane_resolved
       GET "${validation_json}" issues ${event_issue_index}
           eventContext laneResolved)
string(JSON linked_on_current_lane
       GET "${validation_json}" issues ${event_issue_index}
           eventContext linkedSegmentResolved)
string(JSON owner_resolved
       GET "${validation_json}" issues ${event_issue_index}
           eventContext linkedSegmentOwnerResolved)
string(JSON linked_lane_id
       GET "${validation_json}" issues ${event_issue_index}
           eventContext linkedLaneId)
string(JSON linked_lane_name
       GET "${validation_json}" issues ${event_issue_index}
           eventContext linkedLaneName)
string(JSON linked_start
       GET "${validation_json}" issues ${event_issue_index}
           eventContext linkedSegmentStartTick)
string(JSON linked_end
       GET "${validation_json}" issues ${event_issue_index}
           eventContext linkedSegmentEndTick)
string(JSON linked_value
       GET "${validation_json}" issues ${event_issue_index}
           eventContext linkedSegmentValue)
string(JSON link_consistent
       GET "${validation_json}" issues ${event_issue_index}
           eventContext linkConsistent)
string(JSON link_repairable
       GET "${validation_json}" issues ${event_issue_index}
           eventContext linkRepairable)
string(JSON repairable_count
       GET "${validation_json}" repairableIssueCount)
string(JSON repair_target_count
       GET "${validation_json}" repairTargetCount)
math(EXPR expected_repairable "${baseline_repairable} + 1")
math(EXPR expected_repair_targets "${baseline_repair_targets} + 1")
if(NOT issue_path STREQUAL "scenarios[0].events[9].laneId"
   OR NOT path_count EQUAL 3
   OR NOT lane_path STREQUAL "scenarios[0].events[9].laneId"
   OR NOT tick_path STREQUAL "scenarios[0].events[9].timeTick"
   OR NOT value_path STREQUAL "scenarios[0].events[9].value"
   OR NOT repair_property STREQUAL "event-link"
   OR NOT repair_operation STREQUAL "repair-event-link"
   OR NOT delete_operation STREQUAL "delete-event"
   OR NOT event_id STREQUAL "event-ack-high"
   OR NOT lane_id STREQUAL "lane-reset"
   OR NOT lane_resolved
   OR linked_on_current_lane
   OR NOT owner_resolved
   OR NOT linked_lane_id STREQUAL "lane-ack"
   OR NOT linked_lane_name STREQUAL "ack"
   OR NOT linked_start EQUAL 110000
   OR NOT linked_end EQUAL 150000
   OR NOT linked_value STREQUAL "1"
   OR link_consistent
   OR NOT link_repairable
   OR NOT repairable_count EQUAL expected_repairable
   OR NOT repair_target_count EQUAL expected_repair_targets)
    message(FATAL_ERROR "validate returned the wrong Event link repair context")
endif()

file(WRITE "${operations_file}"
"{
  \"schema\": \"wave-workbench.operations/v1\",
  \"scenarioId\": \"scenario-handshake\",
  \"operations\": [
    {
      \"op\": \"repair-event-link\",
      \"eventId\": \"${event_id}\"
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
    message(FATAL_ERROR "Healthy Event link repair was not atomically rejected")
endif()
string(FIND "${healthy_error}" "not reported by validate" healthy_error_position)
if(healthy_error_position EQUAL -1)
    message(FATAL_ERROR "Healthy Event link repair omitted its validation-only cause")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" apply "${broken_project}" "${operations_file}"
            "--output=${dry_output}" --dry-run
    RESULT_VARIABLE dry_code
    OUTPUT_VARIABLE dry_json
    ERROR_VARIABLE dry_error
)
if(NOT dry_code EQUAL 0 OR EXISTS "${dry_output}")
    message(FATAL_ERROR "Event link repair dry-run failed or wrote output: ${dry_error}")
endif()
string(JSON dry_sha GET "${dry_json}" resultSha256)
string(JSON source_errors
       GET "${dry_json}" validationGuard sourceErrorCount)
string(JSON candidate_errors
       GET "${dry_json}" validationGuard candidateErrorCount)
string(JSON guard_reason
       GET "${dry_json}" validationGuard reason)
string(JSON updated_events
       GET "${dry_json}" operations 0 updatedEventCount)
string(JSON created_events
       GET "${dry_json}" operations 0 createdEventCount)
string(JSON removed_events
       GET "${dry_json}" operations 0 removedEventCount)
string(JSON removed_relations
       GET "${dry_json}" operations 0 removedRelationCount)
string(JSON removed_segments
       GET "${dry_json}" operations 0 removedSegmentCount)
string(JSON before_consistent
       GET "${dry_json}" operations 0 beforeEventContext linkConsistent)
string(JSON after_consistent
       GET "${dry_json}" operations 0 eventContext linkConsistent)
string(JSON after_lane
       GET "${dry_json}" operations 0 eventContext laneId)
string(JSON after_tick
       GET "${dry_json}" operations 0 eventContext timeTick)
string(JSON after_value
       GET "${dry_json}" operations 0 eventContext value)
if(NOT source_errors EQUAL 1
   OR NOT candidate_errors EQUAL 0
   OR NOT guard_reason STREQUAL "valid-candidate"
   OR NOT updated_events EQUAL 1
   OR NOT created_events EQUAL 0
   OR NOT removed_events EQUAL 0
   OR NOT removed_relations EQUAL 0
   OR NOT removed_segments EQUAL 0
   OR before_consistent
   OR NOT after_consistent
   OR NOT after_lane STREQUAL "lane-ack"
   OR NOT after_tick EQUAL 110000
   OR NOT after_value STREQUAL "1")
    message(FATAL_ERROR "Event link repair changed the wrong model content")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" apply "${broken_project}" "${operations_file}"
            "--output=${repaired_project}"
    RESULT_VARIABLE apply_code
    OUTPUT_VARIABLE apply_json
    ERROR_VARIABLE apply_error
)
if(NOT apply_code EQUAL 0 OR NOT EXISTS "${repaired_project}")
    message(FATAL_ERROR "Event link repair failed: ${apply_error}")
endif()
string(JSON apply_sha GET "${apply_json}" resultSha256)
file(SHA256 "${repaired_project}" repaired_sha)
if(NOT apply_sha STREQUAL dry_sha
   OR NOT repaired_sha STREQUAL apply_sha)
    message(FATAL_ERROR "Event link repair dry-run and write SHA differ")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" validate "${repaired_project}"
    RESULT_VARIABLE final_code
    OUTPUT_VARIABLE final_json
    ERROR_VARIABLE final_error
)
if(NOT final_code EQUAL 0)
    message(FATAL_ERROR "Event link repair result did not validate: ${final_error}")
endif()
string(JSON final_valid GET "${final_json}" valid)
string(JSON final_errors GET "${final_json}" summary errors)
string(JSON final_repairable GET "${final_json}" repairableIssueCount)
string(JSON final_repair_targets GET "${final_json}" repairTargetCount)
if(NOT final_valid
   OR NOT final_errors EQUAL 0
   OR NOT final_repairable EQUAL baseline_repairable
   OR NOT final_repair_targets EQUAL baseline_repair_targets)
    message(FATAL_ERROR "Event link repair left validation damage")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" relations "${repaired_project}"
            "--match=within"
    RESULT_VARIABLE relation_code
    OUTPUT_VARIABLE relation_json
    ERROR_VARIABLE relation_error
)
if(NOT relation_code EQUAL 0)
    message(FATAL_ERROR "Repaired Relation query failed: ${relation_error}")
endif()
string(JSON relation_count GET "${relation_json}" matchCount)
if(NOT relation_count EQUAL 1)
    message(FATAL_ERROR "Event link repair did not preserve one Relation")
endif()
string(JSON relation_ready GET "${relation_json}" relations 0 endpointsReady)
string(JSON target_lane GET "${relation_json}" relations 0 target laneId)
string(JSON target_tick GET "${relation_json}" relations 0 target timeTick)
string(JSON target_value GET "${relation_json}" relations 0 target value)
if(NOT relation_ready
   OR NOT target_lane STREQUAL "lane-ack"
   OR NOT target_tick EQUAL 110000
   OR NOT target_value STREQUAL "1")
    message(FATAL_ERROR "Event link repair did not preserve the Relation endpoint")
endif()

string(REPLACE
    "\"id\": \"event-data-idle-a\""
    "\"id\": \"event-ack-high\""
    ambiguous_json
    "${broken_json}")
if(ambiguous_json STREQUAL broken_json)
    message(FATAL_ERROR "Ambiguous Event link fixture was not damaged")
endif()
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
    message(FATAL_ERROR "Ambiguous Event link repair was not atomically rejected")
endif()
string(FIND "${ambiguous_error}" "ambiguous" ambiguous_error_position)
if(ambiguous_error_position EQUAL -1)
    message(FATAL_ERROR "Ambiguous Event link repair omitted its cause")
endif()
file(SHA256 "${ambiguous_project}" ambiguous_sha_after)
if(NOT ambiguous_sha_after STREQUAL ambiguous_sha_before)
    message(FATAL_ERROR "Ambiguous Event link repair changed its source")
endif()

file(SHA256 "${PROJECT}" source_sha_after)
file(SHA256 "${broken_project}" broken_sha_after)
if(NOT source_sha_after STREQUAL source_sha_before
   OR NOT broken_sha_after STREQUAL broken_sha_before)
    message(FATAL_ERROR "Event link repair workflow changed a source project")
endif()

if(NOT DEFINED WAVE_CLI
   OR NOT DEFINED PROJECT
   OR NOT DEFINED OUTPUT)
    message(FATAL_ERROR "Event validation context CLI contract smoke is missing an input")
endif()

set(broken_project "${OUTPUT}-broken.wave.json")
set(ambiguous_project "${OUTPUT}-ambiguous.wave.json")
set(repaired_project "${OUTPUT}-repaired.wave.json")
set(dry_output "${OUTPUT}-dry.wave.json")
set(ambiguous_output "${OUTPUT}-ambiguous-output.wave.json")
set(healthy_output "${OUTPUT}-healthy-output.wave.json")
set(operations_file "${OUTPUT}.operations.json")
file(REMOVE
    "${broken_project}"
    "${ambiguous_project}"
    "${repaired_project}"
    "${dry_output}"
    "${ambiguous_output}"
    "${healthy_output}"
    "${operations_file}")

file(SHA256 "${PROJECT}" source_sha_before)
file(READ "${PROJECT}" source_json)
string(REPLACE
    "\"id\": \"event-ack-high\", \"laneId\": \"lane-ack\""
    "\"id\": \"event-ack-high\", \"laneId\": \"lane-missing\""
    broken_json
    "${source_json}")
if(broken_json STREQUAL source_json)
    message(FATAL_ERROR "Event validation context fixture was not damaged")
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
       GET "${capabilities_json}" features structuredEventValidation)
string(JSON deletion_capability
       GET "${capabilities_json}" features eventDeletion)
string(JSON operation_count
       GET "${capabilities_json}" operationCount)
string(JSON operation_length
       LENGTH "${capabilities_json}" operations)
set(has_delete_event FALSE)
math(EXPR operation_last "${operation_length} - 1")
foreach(operation_index RANGE 0 ${operation_last})
    string(JSON operation_name
           GET "${capabilities_json}" operations ${operation_index} name)
    if(operation_name STREQUAL "delete-event")
        set(has_delete_event TRUE)
    endif()
endforeach()
if(NOT structured_capability
   OR NOT deletion_capability
   OR NOT operation_count EQUAL 38
   OR NOT operation_length EQUAL 38
   OR NOT has_delete_event)
    message(FATAL_ERROR "capabilities omit structured Event recovery")
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
    message(FATAL_ERROR "Missing Event Lane did not reject validate: ${validation_error}")
endif()

set(event_issue_index "")
string(JSON issue_count LENGTH "${validation_json}" issues)
math(EXPR last_issue "${issue_count} - 1")
foreach(issue_index RANGE 0 ${last_issue})
    string(JSON issue_code
           GET "${validation_json}" issues ${issue_index} code)
    if(issue_code STREQUAL "missing-lane")
        set(event_issue_index "${issue_index}")
        break()
    endif()
endforeach()
if(event_issue_index STREQUAL "")
    message(FATAL_ERROR "validate omitted the missing Event Lane issue")
endif()

string(JSON object_kind
       GET "${validation_json}" issues ${event_issue_index} objectKind)
string(JSON scenario_index
       GET "${validation_json}" issues ${event_issue_index} scenarioIndex)
string(JSON object_index
       GET "${validation_json}" issues ${event_issue_index} objectIndex)
string(JSON issue_path
       GET "${validation_json}" issues ${event_issue_index} path)
string(JSON repair_property
       GET "${validation_json}" issues ${event_issue_index}
           repairProperties 0)
string(JSON delete_repair_property
       GET "${validation_json}" issues ${event_issue_index}
           repairProperties 1)
string(JSON repair_operation
       GET "${validation_json}" issues ${event_issue_index}
           repairOperations 0)
string(JSON delete_repair_operation
       GET "${validation_json}" issues ${event_issue_index}
           repairOperations 1)
string(JSON event_id
       GET "${validation_json}" issues ${event_issue_index} eventId)
string(JSON event_context
       GET "${validation_json}" issues ${event_issue_index} eventContext)
string(JSON context_event_id
       GET "${validation_json}" issues ${event_issue_index}
           eventContext eventId)
string(JSON addressable
       GET "${validation_json}" issues ${event_issue_index}
           eventContext addressable)
string(JSON lane_id
       GET "${validation_json}" issues ${event_issue_index}
           eventContext laneId)
string(JSON lane_resolved
       GET "${validation_json}" issues ${event_issue_index}
           eventContext laneResolved)
string(JSON time_tick
       GET "${validation_json}" issues ${event_issue_index}
           eventContext timeTick)
string(JSON formatted_time
       GET "${validation_json}" issues ${event_issue_index}
           eventContext time)
string(JSON within_scenario
       GET "${validation_json}" issues ${event_issue_index}
           eventContext withinScenario)
string(JSON action
       GET "${validation_json}" issues ${event_issue_index}
           eventContext action)
string(JSON event_value
       GET "${validation_json}" issues ${event_issue_index}
           eventContext value)
string(JSON clock_resolved
       GET "${validation_json}" issues ${event_issue_index}
           eventContext clockResolved)
string(JSON waveform_linked
       GET "${validation_json}" issues ${event_issue_index}
           eventContext waveformLinked)
string(JSON linked_segment_resolved
       GET "${validation_json}" issues ${event_issue_index}
           eventContext linkedSegmentResolved)
string(JSON linked_owner_resolved
       GET "${validation_json}" issues ${event_issue_index}
           eventContext linkedSegmentOwnerResolved)
string(JSON linked_lane_id
       GET "${validation_json}" issues ${event_issue_index}
           eventContext linkedLaneId)
string(JSON linked_start_tick
       GET "${validation_json}" issues ${event_issue_index}
           eventContext linkedSegmentStartTick)
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
string(FIND "${event_context}" "linkedSegmentId" linked_id_position)
string(FIND "${event_context}" "extensions" extensions_position)
if(NOT object_kind STREQUAL "event"
   OR NOT scenario_index EQUAL 0
   OR NOT object_index EQUAL 9
   OR NOT issue_path STREQUAL "scenarios[0].events[9].laneId"
   OR NOT repair_property STREQUAL "event-link"
   OR NOT delete_repair_property STREQUAL "event"
   OR NOT repair_operation STREQUAL "repair-event-link"
   OR NOT delete_repair_operation STREQUAL "delete-event"
   OR NOT event_id STREQUAL "event-ack-high"
   OR NOT context_event_id STREQUAL event_id
   OR NOT addressable
   OR NOT lane_id STREQUAL "lane-missing"
   OR lane_resolved
   OR NOT time_tick EQUAL 110000
   OR NOT formatted_time STREQUAL "110 ns"
   OR NOT within_scenario
   OR NOT action STREQUAL "expect"
   OR NOT event_value STREQUAL "1"
   OR NOT clock_resolved
   OR NOT waveform_linked
   OR linked_segment_resolved
   OR NOT linked_owner_resolved
   OR NOT linked_lane_id STREQUAL "lane-ack"
   OR NOT linked_start_tick EQUAL 110000
   OR NOT linked_value STREQUAL "1"
   OR link_consistent
   OR NOT link_repairable
   OR NOT linked_id_position EQUAL -1
   OR NOT extensions_position EQUAL -1
   OR NOT repairable_count EQUAL expected_repairable
   OR NOT repair_target_count EQUAL expected_repair_targets)
    message(FATAL_ERROR "validate did not return the structured Event context")
endif()

file(WRITE "${operations_file}"
"{
  \"schema\": \"wave-workbench.operations/v1\",
  \"scenarioId\": \"scenario-handshake\",
  \"operations\": [
    {
      \"op\": \"delete-event\",
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
    message(FATAL_ERROR "Healthy Event deletion was not atomically rejected")
endif()
string(FIND "${healthy_error}" "safely deletable" healthy_error_position)
if(healthy_error_position EQUAL -1)
    message(FATAL_ERROR "Healthy Event deletion omitted its validation-only cause")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" apply "${broken_project}" "${operations_file}"
            "--output=${dry_output}" --dry-run
    RESULT_VARIABLE dry_code
    OUTPUT_VARIABLE dry_json
    ERROR_VARIABLE dry_error
)
if(NOT dry_code EQUAL 0 OR EXISTS "${dry_output}")
    message(FATAL_ERROR "Structured Event dry-run failed or wrote output: ${dry_error}")
endif()
string(JSON dry_sha GET "${dry_json}" resultSha256)
string(JSON dry_source_errors
       GET "${dry_json}" validationGuard sourceErrorCount)
string(JSON dry_candidate_errors
       GET "${dry_json}" validationGuard candidateErrorCount)
string(JSON dry_reason
       GET "${dry_json}" validationGuard reason)
string(JSON removed_events
       GET "${dry_json}" operations 0 removedEventCount)
string(JSON removed_relations
       GET "${dry_json}" operations 0 removedRelationCount)
string(JSON removed_segments
       GET "${dry_json}" operations 0 removedSegmentCount)
if(NOT dry_source_errors EQUAL 1
   OR NOT dry_candidate_errors EQUAL 0
   OR NOT dry_reason STREQUAL "valid-candidate"
   OR NOT removed_events EQUAL 1
   OR NOT removed_relations EQUAL 1
   OR NOT removed_segments EQUAL 0)
    message(FATAL_ERROR "Structured Event repair did not clean dependencies")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" apply "${broken_project}" "${operations_file}"
            "--output=${repaired_project}"
    RESULT_VARIABLE apply_code
    OUTPUT_VARIABLE apply_json
    ERROR_VARIABLE apply_error
)
if(NOT apply_code EQUAL 0 OR NOT EXISTS "${repaired_project}")
    message(FATAL_ERROR "Structured Event apply failed: ${apply_error}")
endif()
string(JSON apply_sha GET "${apply_json}" resultSha256)
file(SHA256 "${repaired_project}" repaired_sha)
if(NOT apply_sha STREQUAL dry_sha
   OR NOT repaired_sha STREQUAL apply_sha)
    message(FATAL_ERROR "Structured Event dry-run and write SHA differ")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" validate "${repaired_project}"
    RESULT_VARIABLE final_code
    OUTPUT_VARIABLE final_json
    ERROR_VARIABLE final_error
)
if(NOT final_code EQUAL 0)
    message(FATAL_ERROR "Structured Event result did not validate: ${final_error}")
endif()
string(JSON final_valid GET "${final_json}" valid)
string(JSON final_errors GET "${final_json}" summary errors)
string(JSON final_repairable GET "${final_json}" repairableIssueCount)
string(JSON final_repair_targets GET "${final_json}" repairTargetCount)
if(NOT final_valid
   OR NOT final_errors EQUAL 0
   OR NOT final_repairable EQUAL baseline_repairable
   OR NOT final_repair_targets EQUAL baseline_repair_targets)
    message(FATAL_ERROR "Structured Event repair left validation damage")
endif()

string(REPLACE
    "\"id\": \"event-data-idle-a\""
    "\"id\": \"event-ack-high\""
    ambiguous_json
    "${source_json}")
if(ambiguous_json STREQUAL source_json)
    message(FATAL_ERROR "Ambiguous Event fixture was not damaged")
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
    message(FATAL_ERROR "Ambiguous Event deletion was not atomically rejected")
endif()
string(FIND "${ambiguous_error}" "ambiguous" ambiguous_error_position)
if(ambiguous_error_position EQUAL -1)
    message(FATAL_ERROR "Ambiguous Event deletion omitted its cause")
endif()
file(SHA256 "${ambiguous_project}" ambiguous_sha_after)
if(NOT ambiguous_sha_after STREQUAL ambiguous_sha_before)
    message(FATAL_ERROR "Ambiguous Event deletion changed its source")
endif()

file(SHA256 "${PROJECT}" source_sha_after)
file(SHA256 "${broken_project}" broken_sha_after)
if(NOT source_sha_after STREQUAL source_sha_before
   OR NOT broken_sha_after STREQUAL broken_sha_before)
    message(FATAL_ERROR "Structured Event workflow changed a source project")
endif()

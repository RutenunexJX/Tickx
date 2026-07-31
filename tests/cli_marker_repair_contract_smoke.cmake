if(NOT DEFINED WAVE_CLI
   OR NOT DEFINED PROJECT
   OR NOT DEFINED OUTPUT)
    message(FATAL_ERROR "Marker repair CLI contract smoke is missing an input")
endif()

set(broken_project "${OUTPUT}-broken.wave.json")
set(repaired_project "${OUTPUT}-repaired.wave.json")
set(deleted_project "${OUTPUT}-deleted.wave.json")
set(ambiguous_output "${OUTPUT}-ambiguous.wave.json")
set(stale_output "${OUTPUT}-stale.wave.json")
set(repair_operations "${OUTPUT}-repair.operations.json")
set(delete_operations "${OUTPUT}-delete.operations.json")
set(ambiguous_operations "${OUTPUT}-ambiguous.operations.json")
file(REMOVE
    "${broken_project}"
    "${repaired_project}"
    "${deleted_project}"
    "${ambiguous_output}"
    "${stale_output}"
    "${repair_operations}"
    "${delete_operations}"
    "${ambiguous_operations}")

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
                    "name": "Transfer follow-up",
                    "startTick": "160000",
                    "endTick": "180000",
                    "kind": "note",
                    "note": "Imported duplicate identity"
                }
]=])
string(REPLACE
    "${original_marker}"
    "${duplicate_markers}"
    broken_json
    "${source_json}")
if(broken_json STREQUAL source_json)
    message(FATAL_ERROR "Marker repair fixture did not create a duplicate ID")
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
string(JSON marker_repair_capability
       GET "${capabilities_json}" features markerRepairReference)
if(NOT marker_repair_capability)
    message(FATAL_ERROR "capabilities omit Marker repair references")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" markers "${broken_project}"
            --match=marker-transfer --exact
    RESULT_VARIABLE query_code
    OUTPUT_VARIABLE query_json
    ERROR_VARIABLE query_error
)
if(NOT query_code EQUAL 0)
    message(FATAL_ERROR "duplicate Marker query failed: ${query_error}")
endif()
string(JSON match_count GET "${query_json}" matchCount)
string(JSON addressable_count GET "${query_json}" addressableCount)
string(JSON first_id_count GET "${query_json}" markers 0 idCount)
string(JSON first_ref GET "${query_json}" markers 0 markerRef)
string(JSON second_ref GET "${query_json}" markers 1 markerRef)
string(JSON second_name GET "${query_json}" markers 1 name)
if(NOT match_count EQUAL 2
   OR NOT addressable_count EQUAL 0
   OR NOT first_id_count EQUAL 2
   OR first_ref STREQUAL ""
   OR second_ref STREQUAL ""
   OR first_ref STREQUAL second_ref
   OR NOT second_name STREQUAL "Transfer follow-up")
    message(FATAL_ERROR "duplicate Marker query did not return distinct repair references")
endif()

file(WRITE "${ambiguous_operations}" [=[
{
  "schema": "wave-workbench.operations/v1",
  "scenarioId": "scenario-handshake",
  "operations": [
    {
      "op": "update-marker",
      "markerId": "marker-transfer",
      "note": "Must not select the first duplicate"
    }
  ]
}
]=])
execute_process(
    COMMAND "${WAVE_CLI}" apply "${broken_project}" "${ambiguous_operations}"
            "--output=${ambiguous_output}"
    RESULT_VARIABLE ambiguous_code
    OUTPUT_VARIABLE ambiguous_stdout
    ERROR_VARIABLE ambiguous_json
)
if(NOT ambiguous_code EQUAL 4
   OR NOT ambiguous_stdout STREQUAL ""
   OR EXISTS "${ambiguous_output}")
    message(FATAL_ERROR "ambiguous Marker ID was accepted or wrote output")
endif()
string(JSON ambiguous_error_code GET "${ambiguous_json}" error code)
string(JSON ambiguous_message GET "${ambiguous_json}" error message)
string(FIND "${ambiguous_message}" "stable ID" stable_id_position)
string(FIND "${ambiguous_message}" "ambiguous" ambiguous_position)
if(NOT ambiguous_error_code STREQUAL "operation-rejected"
   OR stable_id_position EQUAL -1
   OR ambiguous_position EQUAL -1)
    message(FATAL_ERROR "ambiguous Marker ID returned the wrong failure")
endif()

file(WRITE "${repair_operations}"
"{
  \"schema\": \"wave-workbench.operations/v1\",
  \"scenarioId\": \"scenario-handshake\",
  \"operations\": [
    {
      \"op\": \"update-marker\",
      \"markerRef\": \"${second_ref}\",
      \"newId\": \"marker-transfer-follow-up\",
      \"note\": \"Recovered duplicate identity\"
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
    message(FATAL_ERROR "Marker repair dry-run failed or wrote output: ${dry_error}")
endif()
string(JSON dry_sha GET "${dry_json}" resultSha256)
string(JSON dry_changed GET "${dry_json}" operations 0 changed)
string(JSON selected_by_ref
       GET "${dry_json}" operations 0 selectedByRepairRef)
string(JSON repaired_identity
       GET "${dry_json}" operations 0 repairedIdentity)
string(JSON repaired_geometry
       GET "${dry_json}" operations 0 repairedGeometry)
string(JSON previous_id
       GET "${dry_json}" operations 0 previousMarkerId)
string(JSON repaired_id GET "${dry_json}" operations 0 markerId)
string(JSON replacement_ref GET "${dry_json}" operations 0 markerRef)
if(NOT dry_changed
   OR NOT selected_by_ref
   OR NOT repaired_identity
   OR repaired_geometry
   OR NOT previous_id STREQUAL "marker-transfer"
   OR NOT repaired_id STREQUAL "marker-transfer-follow-up"
   OR replacement_ref STREQUAL second_ref)
    message(FATAL_ERROR "Marker repair dry-run returned the wrong contract")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" apply "${broken_project}" "${repair_operations}"
            "--output=${repaired_project}"
    RESULT_VARIABLE repair_code
    OUTPUT_VARIABLE repair_json
    ERROR_VARIABLE repair_error
)
if(NOT repair_code EQUAL 0 OR NOT EXISTS "${repaired_project}")
    message(FATAL_ERROR "Marker repair write failed: ${repair_error}")
endif()
string(JSON repair_sha GET "${repair_json}" resultSha256)
if(NOT repair_sha STREQUAL dry_sha)
    message(FATAL_ERROR "Marker repair dry-run and write SHA differ")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" markers "${repaired_project}"
            --match=marker-transfer --exact
    RESULT_VARIABLE old_query_code
    OUTPUT_VARIABLE old_query_json
    ERROR_VARIABLE old_query_error
)
execute_process(
    COMMAND "${WAVE_CLI}" markers "${repaired_project}"
            --match=marker-transfer-follow-up --exact
    RESULT_VARIABLE new_query_code
    OUTPUT_VARIABLE new_query_json
    ERROR_VARIABLE new_query_error
)
if(NOT old_query_code EQUAL 0 OR NOT new_query_code EQUAL 0)
    message(FATAL_ERROR "repaired Marker query failed: ${old_query_error}${new_query_error}")
endif()
string(JSON old_match_count GET "${old_query_json}" matchCount)
string(JSON old_addressable GET "${old_query_json}" addressableCount)
string(JSON new_match_count GET "${new_query_json}" matchCount)
string(JSON new_addressable GET "${new_query_json}" addressableCount)
string(JSON new_note GET "${new_query_json}" markers 0 note)
if(NOT old_match_count EQUAL 1
   OR NOT old_addressable EQUAL 1
   OR NOT new_match_count EQUAL 1
   OR NOT new_addressable EQUAL 1
   OR NOT new_note STREQUAL "Recovered duplicate identity")
    message(FATAL_ERROR "Marker identity repair did not make both objects addressable")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" validate "${repaired_project}"
            --scenario=scenario-handshake
    RESULT_VARIABLE validate_code
    OUTPUT_VARIABLE validate_json
    ERROR_VARIABLE validate_error
)
if(NOT validate_code EQUAL 0)
    message(FATAL_ERROR "repaired Marker validation failed: ${validate_error}")
endif()
string(JSON validation_errors GET "${validate_json}" summary errors)
if(NOT validation_errors STREQUAL "0")
    message(FATAL_ERROR "Marker identity repair left validation errors")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" apply "${repaired_project}" "${repair_operations}"
            "--output=${stale_output}"
    RESULT_VARIABLE stale_code
    OUTPUT_VARIABLE stale_stdout
    ERROR_VARIABLE stale_json
)
if(NOT stale_code EQUAL 4
   OR NOT stale_stdout STREQUAL ""
   OR EXISTS "${stale_output}")
    message(FATAL_ERROR "stale Marker repair reference was accepted")
endif()
string(JSON stale_message GET "${stale_json}" error message)
string(FIND "${stale_message}" "stale" stale_position)
if(stale_position EQUAL -1)
    message(FATAL_ERROR "stale Marker repair reference returned the wrong failure")
endif()

file(WRITE "${delete_operations}"
"{
  \"schema\": \"wave-workbench.operations/v1\",
  \"scenarioId\": \"scenario-handshake\",
  \"operations\": [
    {
      \"op\": \"delete-marker\",
      \"markerRef\": \"${second_ref}\"
    }
  ]
}
")
execute_process(
    COMMAND "${WAVE_CLI}" apply "${broken_project}" "${delete_operations}"
            "--output=${deleted_project}"
    RESULT_VARIABLE delete_code
    OUTPUT_VARIABLE delete_json
    ERROR_VARIABLE delete_error
)
if(NOT delete_code EQUAL 0 OR NOT EXISTS "${deleted_project}")
    message(FATAL_ERROR "delete-marker by repair reference failed: ${delete_error}")
endif()
string(JSON delete_selected_by_ref
       GET "${delete_json}" operations 0 selectedByRepairRef)
execute_process(
    COMMAND "${WAVE_CLI}" markers "${deleted_project}"
            --match=marker-transfer --exact
    RESULT_VARIABLE deleted_query_code
    OUTPUT_VARIABLE deleted_query_json
    ERROR_VARIABLE deleted_query_error
)
if(NOT deleted_query_code EQUAL 0)
    message(FATAL_ERROR "deleted Marker re-query failed: ${deleted_query_error}")
endif()
string(JSON deleted_match_count GET "${deleted_query_json}" matchCount)
string(JSON deleted_addressable GET "${deleted_query_json}" addressableCount)
if(NOT delete_selected_by_ref
   OR NOT deleted_match_count EQUAL 1
   OR NOT deleted_addressable EQUAL 1)
    message(FATAL_ERROR "delete-marker did not remove exactly one duplicate")
endif()

file(SHA256 "${PROJECT}" source_sha_after)
file(SHA256 "${broken_project}" broken_sha_after)
if(NOT source_sha_after STREQUAL source_sha_before
   OR NOT broken_sha_after STREQUAL broken_sha_before)
    message(FATAL_ERROR "Marker repair changed a source project")
endif()

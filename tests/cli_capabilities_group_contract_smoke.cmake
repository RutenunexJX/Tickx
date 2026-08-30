if(NOT DEFINED WAVE_CLI
   OR NOT DEFINED GROUP_OPERATIONS
   OR NOT DEFINED CLEANUP_OPERATIONS
   OR NOT DEFINED INVALID_OPERATIONS
   OR NOT DEFINED OUTPUT)
    message(FATAL_ERROR "Capabilities/Group CLI contract smoke is missing an input")
endif()

set(source_output "${OUTPUT}.wave.json")
set(group_output "${OUTPUT}-group.wave.json")
set(cleanup_output "${OUTPUT}-cleanup.wave.json")
set(invalid_output "${OUTPUT}-invalid.wave.json")
file(REMOVE
    "${source_output}"
    "${group_output}"
    "${cleanup_output}"
    "${invalid_output}")

execute_process(
    COMMAND "${WAVE_CLI}" capabilities --pretty
    RESULT_VARIABLE capabilities_code
    OUTPUT_VARIABLE capabilities_json
    ERROR_VARIABLE capabilities_error
)
if(NOT capabilities_code EQUAL 0)
    message(FATAL_ERROR "capabilities failed: ${capabilities_error}")
endif()
string(JSON capabilities_schema GET "${capabilities_json}" schema)
string(JSON command_count GET "${capabilities_json}" commandCount)
string(JSON operation_count GET "${capabilities_json}" operationCount)
string(JSON operation_length LENGTH "${capabilities_json}" operations)
string(JSON batch_atomic GET "${capabilities_json}" operationBatch atomic)
string(JSON report_schema GET "${capabilities_json}" reportSchema)
string(JSON batch_schema GET "${capabilities_json}" operationBatch schema)
string(JSON relation_selector_length
       LENGTH "${capabilities_json}" selectors relationId)
string(JSON marker_selector_length
       LENGTH "${capabilities_json}" selectors markerId)
string(JSON time_format_count LENGTH "${capabilities_json}" time formats)
string(JSON structured_errors
       GET "${capabilities_json}" features structuredErrors)
string(JSON relation_ready_edges
       GET "${capabilities_json}" features relationReadyEdgeQuery)
string(JSON compact_relation_query
       GET "${capabilities_json}" features compactRelationQuery)
string(JSON edge_kind_count
       LENGTH "${capabilities_json}" values edgeKinds)
set(has_edges FALSE)
set(has_relations FALSE)
math(EXPR command_last "${command_count} - 1")
foreach(command_index RANGE 0 ${command_last})
    string(JSON command_name
           GET "${capabilities_json}" commands ${command_index} name)
    if(command_name STREQUAL "edges")
        set(has_edges TRUE)
    elseif(command_name STREQUAL "relations")
        set(has_relations TRUE)
    endif()
endforeach()
set(has_update_group FALSE)
set(has_move_group FALSE)
set(has_delete_group FALSE)
set(has_create_scenario FALSE)
set(has_duplicate_scenario FALSE)
set(has_rename_scenario FALSE)
set(has_delete_scenario FALSE)
set(has_reorder_scenario FALSE)
math(EXPR operation_last "${operation_length} - 1")
foreach(operation_index RANGE 0 ${operation_last})
    string(JSON operation_name
           GET "${capabilities_json}" operations ${operation_index} name)
    if(operation_name STREQUAL "update-group")
        set(has_update_group TRUE)
    elseif(operation_name STREQUAL "move-group")
        set(has_move_group TRUE)
    elseif(operation_name STREQUAL "delete-group")
        set(has_delete_group TRUE)
    elseif(operation_name STREQUAL "create-scenario")
        set(has_create_scenario TRUE)
    elseif(operation_name STREQUAL "duplicate-scenario")
        set(has_duplicate_scenario TRUE)
    elseif(operation_name STREQUAL "rename-scenario")
        set(has_rename_scenario TRUE)
    elseif(operation_name STREQUAL "delete-scenario")
        set(has_delete_scenario TRUE)
    elseif(operation_name STREQUAL "reorder-scenario")
        set(has_reorder_scenario TRUE)
    endif()
endforeach()
if(NOT capabilities_schema STREQUAL "wave-workbench.capabilities/v1"
   OR NOT report_schema STREQUAL "wave-workbench.cli/v1"
   OR NOT batch_schema STREQUAL "wave-workbench.operations/v1"
   OR NOT command_count EQUAL 11
   OR NOT operation_count EQUAL 38
   OR NOT operation_length EQUAL 38
   OR NOT batch_atomic
   OR NOT relation_selector_length EQUAL 1
   OR NOT marker_selector_length EQUAL 2
   OR NOT time_format_count EQUAL 3
   OR NOT structured_errors
   OR NOT relation_ready_edges
   OR NOT compact_relation_query
   OR NOT edge_kind_count EQUAL 4
   OR NOT has_edges
   OR NOT has_relations
   OR NOT has_update_group
   OR NOT has_move_group
   OR NOT has_delete_group
   OR NOT has_create_scenario
   OR NOT has_duplicate_scenario
   OR NOT has_rename_scenario
   OR NOT has_delete_scenario
   OR NOT has_reorder_scenario)
    message(FATAL_ERROR "capabilities returned an incomplete contract")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" capabilities --unknown
    RESULT_VARIABLE invalid_capabilities_code
    OUTPUT_VARIABLE invalid_capabilities_output
    ERROR_VARIABLE invalid_capabilities_json
)
if(NOT invalid_capabilities_code EQUAL 2)
    message(FATAL_ERROR "capabilities accepted an unknown option")
endif()
string(JSON invalid_capabilities_error
       GET "${invalid_capabilities_json}" error code)
if(NOT invalid_capabilities_error STREQUAL "usage")
    message(FATAL_ERROR "invalid capabilities option was not a structured usage error")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" new "${OUTPUT}"
            "--name=Group lifecycle"
            "--duration=40 ns"
    RESULT_VARIABLE new_code
    OUTPUT_VARIABLE new_json
    ERROR_VARIABLE new_error
)
if(NOT new_code EQUAL 0 OR NOT EXISTS "${source_output}")
    message(FATAL_ERROR "Group lifecycle source creation failed: ${new_error}")
endif()
string(JSON source_sha GET "${new_json}" resultSha256)

execute_process(
    COMMAND "${WAVE_CLI}" apply "${source_output}" "${GROUP_OPERATIONS}"
            "--output=${group_output}" --dry-run
    RESULT_VARIABLE dry_code
    OUTPUT_VARIABLE dry_json
    ERROR_VARIABLE dry_error
)
if(NOT dry_code EQUAL 0 OR EXISTS "${group_output}")
    message(FATAL_ERROR "Group lifecycle dry-run failed: ${dry_error}")
endif()
string(JSON dry_sha GET "${dry_json}" resultSha256)
string(JSON group_operation_count GET "${dry_json}" operationCount)
string(JSON group_id GET "${dry_json}" operations 0 groupId)
string(JSON updated_group_id GET "${dry_json}" operations 3 groupId)
string(JSON updated_name GET "${dry_json}" operations 3 name)
string(JSON updated_color GET "${dry_json}" operations 3 color)
string(JSON updated_height GET "${dry_json}" operations 3 height)
string(JSON updated_visible GET "${dry_json}" operations 3 visible)
string(JSON updated_members GET "${dry_json}" operations 3 memberCount)
string(JSON moved_destination GET "${dry_json}" operations 4 destinationIndex)
string(JSON update_no_effect GET "${dry_json}" operations 5 changed)
string(JSON move_no_effect GET "${dry_json}" operations 6 changed)
string(JSON after_lane_count
       GET "${dry_json}" changes scenarios 0 afterLaneCount)
if(NOT group_operation_count EQUAL 7
   OR NOT group_id STREQUAL "${updated_group_id}"
   OR NOT updated_name STREQUAL "Control"
   OR NOT updated_color STREQUAL "#336699"
   OR NOT updated_height EQUAL 52
   OR updated_visible
   OR NOT updated_members EQUAL 1
   OR NOT moved_destination EQUAL 2
   OR update_no_effect
   OR move_no_effect
   OR NOT after_lane_count EQUAL 3)
    message(FATAL_ERROR "Group lifecycle dry-run returned the wrong result")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" apply "${source_output}" "${GROUP_OPERATIONS}"
            "--output=${group_output}"
    RESULT_VARIABLE group_code
    OUTPUT_VARIABLE group_json
    ERROR_VARIABLE group_error
)
if(NOT group_code EQUAL 0 OR NOT EXISTS "${group_output}")
    message(FATAL_ERROR "Group lifecycle write failed: ${group_error}")
endif()
string(JSON group_sha GET "${group_json}" resultSha256)
if(NOT group_sha STREQUAL "${dry_sha}")
    message(FATAL_ERROR "Group lifecycle dry-run and write SHA differ")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" signals "${group_output}"
            "--match=req" --kind=bit --exact
    RESULT_VARIABLE member_code
    OUTPUT_VARIABLE member_json
    ERROR_VARIABLE member_error
)
if(NOT member_code EQUAL 0)
    message(FATAL_ERROR "Group member query failed: ${member_error}")
endif()
string(JSON member_group_id GET "${member_json}" signals 0 groupId)
string(JSON member_group_name GET "${member_json}" signals 0 groupName)
if(NOT member_group_id STREQUAL "${group_id}"
   OR NOT member_group_name STREQUAL "Control")
    message(FATAL_ERROR "renamed Group did not retain its member")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" apply "${group_output}" "${CLEANUP_OPERATIONS}"
            "--output=${cleanup_output}"
    RESULT_VARIABLE cleanup_code
    OUTPUT_VARIABLE cleanup_json
    ERROR_VARIABLE cleanup_error
)
if(NOT cleanup_code EQUAL 0 OR NOT EXISTS "${cleanup_output}")
    message(FATAL_ERROR "Group cleanup failed: ${cleanup_error}")
endif()
string(JSON cleanup_group_id GET "${cleanup_json}" operations 0 groupId)
string(JSON cleanup_member_count
       GET "${cleanup_json}" operations 0 ungroupedSignalCount)
string(JSON cleanup_member_id
       GET "${cleanup_json}" operations 0 ungroupedLaneIds 0)
string(JSON cleanup_lane_count
       GET "${cleanup_json}" changes scenarios 0 afterLaneCount)
if(NOT cleanup_group_id STREQUAL "${group_id}"
   OR NOT cleanup_member_count EQUAL 1
   OR NOT cleanup_member_id MATCHES "^lane-"
   OR NOT cleanup_lane_count EQUAL 2)
    message(FATAL_ERROR "delete-group returned the wrong dependency report")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" signals "${cleanup_output}"
            "--match=req" --kind=bit --exact
    RESULT_VARIABLE ungrouped_code
    OUTPUT_VARIABLE ungrouped_json
    ERROR_VARIABLE ungrouped_error
)
if(NOT ungrouped_code EQUAL 0)
    message(FATAL_ERROR "ungrouped member query failed: ${ungrouped_error}")
endif()
string(JSON member_group_after GET "${ungrouped_json}" signals 0 groupId)
if(NOT member_group_after STREQUAL "")
    message(FATAL_ERROR "delete-group did not preserve and ungroup its member")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" apply "${group_output}" "${INVALID_OPERATIONS}"
            "--output=${invalid_output}"
    RESULT_VARIABLE invalid_code
    OUTPUT_VARIABLE invalid_output_json
    ERROR_VARIABLE invalid_json
)
if(NOT invalid_code EQUAL 4 OR EXISTS "${invalid_output}")
    message(FATAL_ERROR "invalid Group target was accepted or wrote an output")
endif()
string(JSON invalid_error_code GET "${invalid_json}" error code)
string(JSON invalid_operation GET "${invalid_json}" error operation)
if(NOT invalid_error_code STREQUAL "operation-rejected"
   OR NOT invalid_operation EQUAL 1)
    message(FATAL_ERROR "invalid Group batch returned the wrong failure contract")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" signals "${group_output}"
            "--match=req" --kind=bit --exact
    RESULT_VARIABLE source_check_code
    OUTPUT_VARIABLE source_check_json
    ERROR_VARIABLE source_check_error
)
if(NOT source_check_code EQUAL 0)
    message(FATAL_ERROR "Group source re-query failed: ${source_check_error}")
endif()
string(JSON source_after_sha GET "${source_check_json}" sourceSha256)
if(NOT source_after_sha STREQUAL "${group_sha}")
    message(FATAL_ERROR "rejected Group batch changed the source project")
endif()

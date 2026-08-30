if(NOT DEFINED WAVE_CLI
   OR NOT DEFINED WAVE_GENERATE
   OR NOT DEFINED WAVE_COMPARE
   OR NOT DEFINED WAVE_BRIDGE
   OR NOT DEFINED PROJECT
   OR NOT DEFINED SIGNALS
   OR NOT DEFINED ARTIFACT_DIR
   OR NOT DEFINED OUTPUT)
    message(FATAL_ERROR
        "Scenario selection CLI contract smoke is missing an input")
endif()

set(multi_project "${OUTPUT}-multi.wave.json")
set(multi_missing_trace_project
    "${OUTPUT}-multi-missing-trace.wave.json")
set(duplicate_id_project "${OUTPUT}-duplicate-id.wave.json")
set(ambiguous_name_project "${OUTPUT}-ambiguous-name.wave.json")
set(imported_project "${OUTPUT}-imported.wave.json")
set(workspace_manifest "${OUTPUT}-workspace.json")
set(compare_implicit_dir "${OUTPUT}-compare-implicit")
set(compare_id_dir "${OUTPUT}-compare-id")
set(compare_name_dir "${OUTPUT}-compare-name")
set(compare_missing_dir "${OUTPUT}-compare-missing")
set(compare_duplicate_dir "${OUTPUT}-compare-duplicate")
set(compare_ambiguous_dir "${OUTPUT}-compare-ambiguous")
set(generate_implicit_dir "${OUTPUT}-generate-implicit")
set(generate_id_dir "${OUTPUT}-generate-id")
set(generate_legacy_dir "${OUTPUT}-generate-legacy")
set(generate_duplicate_dir "${OUTPUT}-generate-duplicate")
set(bridge_implicit_output "${OUTPUT}-bridge-implicit.wave.json")
set(pinloom_implicit_output "${OUTPUT}-pinloom-implicit.json")
set(pinloom_output "${OUTPUT}-pinloom.json")
set(missing_signals "${OUTPUT}-missing-signals.json")
set(missing_artifact_dir "${OUTPUT}-missing-artifacts")
set(lifecycle_operations "${OUTPUT}-lifecycle-operations.json")
set(lifecycle_invalid_operations "${OUTPUT}-lifecycle-invalid-operations.json")
set(lifecycle_result "${OUTPUT}-lifecycle-result.wave.json")
set(lifecycle_invalid_result "${OUTPUT}-lifecycle-invalid-result.wave.json")
set(lifecycle_last_result "${OUTPUT}-lifecycle-last-result.wave.json")

file(REMOVE
    "${multi_project}"
    "${multi_missing_trace_project}"
    "${duplicate_id_project}"
    "${ambiguous_name_project}"
    "${imported_project}"
    "${workspace_manifest}"
    "${bridge_implicit_output}"
    "${pinloom_implicit_output}"
    "${pinloom_output}"
    "${missing_signals}"
    "${lifecycle_operations}"
    "${lifecycle_invalid_operations}"
    "${lifecycle_result}"
    "${lifecycle_invalid_result}"
    "${lifecycle_last_result}")
file(REMOVE_RECURSE
    "${compare_implicit_dir}"
    "${compare_id_dir}"
    "${compare_name_dir}"
    "${compare_missing_dir}"
    "${compare_duplicate_dir}"
    "${compare_ambiguous_dir}"
    "${generate_implicit_dir}"
    "${generate_id_dir}"
    "${generate_legacy_dir}"
    "${generate_duplicate_dir}"
    "${missing_artifact_dir}")

file(SHA256 "${PROJECT}" source_sha_before)
file(READ "${PROJECT}" source_json)
get_filename_component(project_directory "${PROJECT}" DIRECTORY)
file(TO_CMAKE_PATH
    "${project_directory}/traces/handshake_actual.vcd"
    trace_path)
string(JSON source_json
       SET "${source_json}" importedTraces 0 path
       "\"${trace_path}\"")
string(JSON first_scenario
       GET "${source_json}" scenarios 0)
string(JSON alternative_scenario
       SET "${first_scenario}" id
       "\"scenario-alternative\"")
string(JSON alternative_scenario
       SET "${alternative_scenario}" name
       "\"Alternative timing\"")
string(JSON multi_json
       SET "${source_json}" scenarios 1
       "${alternative_scenario}")
file(WRITE "${multi_project}" "${multi_json}")
file(TO_CMAKE_PATH
    "${OUTPUT}-missing-trace.vcd"
    missing_trace_path)
string(JSON multi_missing_trace_json
       SET "${multi_json}" importedTraces 0 path
       "\"${missing_trace_path}\"")
file(WRITE
    "${multi_missing_trace_project}"
    "${multi_missing_trace_json}")

string(JSON duplicate_id_scenario
       SET "${alternative_scenario}" id
       "\"scenario-handshake\"")
string(JSON duplicate_id_json
       SET "${source_json}" scenarios 1
       "${duplicate_id_scenario}")
file(WRITE "${duplicate_id_project}" "${duplicate_id_json}")

string(JSON ambiguous_name_scenario
       SET "${alternative_scenario}" name
       "\"Request / acknowledge\"")
string(JSON ambiguous_name_json
       SET "${source_json}" scenarios 1
       "${ambiguous_name_scenario}")
file(WRITE "${ambiguous_name_project}" "${ambiguous_name_json}")

execute_process(
    COMMAND "${WAVE_BRIDGE}" describe
            "${multi_project}" "${workspace_manifest}"
    RESULT_VARIABLE describe_code
    OUTPUT_VARIABLE describe_output
    ERROR_VARIABLE describe_error
)
if(NOT describe_code EQUAL 0
   OR NOT EXISTS "${workspace_manifest}")
    message(FATAL_ERROR
        "wave-bridge describe failed for a multi-Scenario project: "
        "${describe_output}${describe_error}")
endif()
file(READ "${workspace_manifest}" workspace_json)
string(JSON generate_template
       GET "${workspace_json}" cli generate)
string(JSON compare_template
       GET "${workspace_json}" cli compare)
if(NOT generate_template MATCHES "--scenario=SELECTOR"
   OR NOT compare_template MATCHES "--scenario=SELECTOR")
    message(FATAL_ERROR
        "Workspace manifest advertised ambiguous Scenario-level commands")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" sample "${multi_project}"
            "--lane=req" "--at=0 ns"
    RESULT_VARIABLE cli_implicit_code
    OUTPUT_VARIABLE cli_implicit_output
    ERROR_VARIABLE cli_implicit_json
)
if(NOT cli_implicit_code EQUAL 4)
    message(FATAL_ERROR
        "wave-cli silently selected a Scenario")
endif()
string(JSON cli_implicit_error_code
       GET "${cli_implicit_json}" error code)
string(JSON cli_implicit_error_message
       GET "${cli_implicit_json}" error message)
if(NOT cli_implicit_error_code STREQUAL "selector-invalid"
   OR NOT cli_implicit_error_message MATCHES "contains 2 scenarios"
   OR NOT cli_implicit_error_message MATCHES "--scenario")
    message(FATAL_ERROR
        "wave-cli returned the wrong multiple-Scenario diagnostic")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" sample "${multi_project}"
            "--scenario=alternative TIMING"
            "--lane=req" "--at=0 ns"
    RESULT_VARIABLE cli_name_code
    OUTPUT_VARIABLE cli_name_json
    ERROR_VARIABLE cli_name_error
)
if(NOT cli_name_code EQUAL 0)
    message(FATAL_ERROR
        "wave-cli unique-name selection failed: ${cli_name_error}")
endif()
string(JSON cli_name_scenario_id
       GET "${cli_name_json}" scenarioId)
if(NOT cli_name_scenario_id STREQUAL "scenario-alternative")
    message(FATAL_ERROR
        "wave-cli sampled the wrong Scenario")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" sample "${duplicate_id_project}"
            "--scenario=scenario-handshake"
            "--lane=req" "--at=0 ns"
    RESULT_VARIABLE cli_duplicate_code
    OUTPUT_VARIABLE cli_duplicate_output
    ERROR_VARIABLE cli_duplicate_json
)
if(NOT cli_duplicate_code EQUAL 4)
    message(FATAL_ERROR
        "wave-cli accepted a duplicate Scenario stable ID")
endif()
string(JSON cli_duplicate_error_code
       GET "${cli_duplicate_json}" error code)
string(JSON cli_duplicate_error_message
       GET "${cli_duplicate_json}" error message)
if(NOT cli_duplicate_error_code STREQUAL "selector-invalid"
   OR NOT cli_duplicate_error_message MATCHES "ambiguous")
    message(FATAL_ERROR
        "wave-cli returned the wrong duplicate-ID diagnostic")
endif()

execute_process(
    COMMAND "${WAVE_COMPARE}" "${multi_missing_trace_project}"
            "${compare_implicit_dir}"
    RESULT_VARIABLE compare_implicit_code
    OUTPUT_VARIABLE compare_implicit_output
    ERROR_VARIABLE compare_implicit_error
)
if(NOT compare_implicit_code EQUAL 2
   OR NOT compare_implicit_error MATCHES "contains 2 scenarios"
   OR NOT compare_implicit_error MATCHES "--scenario"
   OR EXISTS "${compare_implicit_dir}")
    message(FATAL_ERROR
        "wave-compare silently selected a Scenario: "
        "${compare_implicit_output}${compare_implicit_error}")
endif()

execute_process(
    COMMAND "${WAVE_COMPARE}" "${multi_project}"
            "${compare_id_dir}"
            "--scenario=scenario-alternative"
    RESULT_VARIABLE compare_id_code
    OUTPUT_VARIABLE compare_id_output
    ERROR_VARIABLE compare_id_error
)
if(NOT compare_id_code EQUAL 0
   OR NOT compare_id_output MATCHES
          "Alternative timing \\(scenario-alternative\\)"
   OR NOT EXISTS
          "${compare_id_dir}/Alternative_timing.compare.json")
    message(FATAL_ERROR
        "wave-compare stable-ID selection failed: "
        "${compare_id_output}${compare_id_error}")
endif()

execute_process(
    COMMAND "${WAVE_COMPARE}" "${multi_project}"
            "${compare_name_dir}"
            "--scenario=alternative TIMING"
    RESULT_VARIABLE compare_name_code
    OUTPUT_VARIABLE compare_name_output
    ERROR_VARIABLE compare_name_error
)
if(NOT compare_name_code EQUAL 0
   OR NOT compare_name_output MATCHES
          "Alternative timing \\(scenario-alternative\\)"
   OR NOT EXISTS
          "${compare_name_dir}/Alternative_timing.compare.json")
    message(FATAL_ERROR
        "wave-compare unique-name selection failed: "
        "${compare_name_output}${compare_name_error}")
endif()

execute_process(
    COMMAND "${WAVE_COMPARE}" "${multi_project}"
            "${compare_missing_dir}"
            "--scenario=missing"
    RESULT_VARIABLE compare_missing_code
    OUTPUT_VARIABLE compare_missing_output
    ERROR_VARIABLE compare_missing_error
)
if(NOT compare_missing_code EQUAL 2
   OR NOT compare_missing_error MATCHES "does not exist"
   OR EXISTS "${compare_missing_dir}")
    message(FATAL_ERROR
        "wave-compare accepted a missing Scenario selector")
endif()

execute_process(
    COMMAND "${WAVE_COMPARE}" "${duplicate_id_project}"
            "${compare_duplicate_dir}"
            "--scenario=scenario-handshake"
    RESULT_VARIABLE compare_duplicate_code
    OUTPUT_VARIABLE compare_duplicate_output
    ERROR_VARIABLE compare_duplicate_error
)
if(NOT compare_duplicate_code EQUAL 2
   OR NOT compare_duplicate_error MATCHES "ambiguous"
   OR EXISTS "${compare_duplicate_dir}")
    message(FATAL_ERROR
        "wave-compare accepted a duplicate Scenario stable ID")
endif()

execute_process(
    COMMAND "${WAVE_COMPARE}" "${ambiguous_name_project}"
            "${compare_ambiguous_dir}"
            "--scenario=Request / acknowledge"
    RESULT_VARIABLE compare_ambiguous_code
    OUTPUT_VARIABLE compare_ambiguous_output
    ERROR_VARIABLE compare_ambiguous_error
)
if(NOT compare_ambiguous_code EQUAL 2
   OR NOT compare_ambiguous_error MATCHES "ambiguous"
   OR EXISTS "${compare_ambiguous_dir}")
    message(FATAL_ERROR
        "wave-compare accepted an ambiguous Scenario name")
endif()

execute_process(
    COMMAND "${WAVE_GENERATE}" "${multi_project}"
            "${generate_implicit_dir}"
    RESULT_VARIABLE generate_implicit_code
    OUTPUT_VARIABLE generate_implicit_output
    ERROR_VARIABLE generate_implicit_error
)
if(NOT generate_implicit_code EQUAL 2
   OR NOT generate_implicit_error MATCHES "contains 2 scenarios"
   OR EXISTS "${generate_implicit_dir}")
    message(FATAL_ERROR
        "wave-generate silently selected a Scenario")
endif()

execute_process(
    COMMAND "${WAVE_GENERATE}" "${multi_project}"
            "${generate_id_dir}"
            "--scenario=scenario-alternative"
    RESULT_VARIABLE generate_id_code
    OUTPUT_VARIABLE generate_id_output
    ERROR_VARIABLE generate_id_error
)
if(NOT generate_id_code EQUAL 0
   OR NOT generate_id_output MATCHES
          "Alternative timing \\(scenario-alternative\\)"
   OR NOT EXISTS "${generate_id_dir}")
    message(FATAL_ERROR
        "wave-generate stable-ID selection failed: "
        "${generate_id_output}${generate_id_error}")
endif()
file(GLOB generated_id_artifacts
     LIST_DIRECTORIES FALSE
     "${generate_id_dir}/*")
list(LENGTH generated_id_artifacts
     generated_id_artifact_count)
if(NOT generated_id_artifact_count EQUAL 7)
    message(FATAL_ERROR
        "wave-generate did not write the seven-artifact bundle")
endif()

execute_process(
    COMMAND "${WAVE_GENERATE}" "${multi_project}"
            "${generate_legacy_dir}"
            "scenario-alternative"
    RESULT_VARIABLE generate_legacy_code
    OUTPUT_VARIABLE generate_legacy_output
    ERROR_VARIABLE generate_legacy_error
)
if(NOT generate_legacy_code EQUAL 0
   OR NOT generate_legacy_output MATCHES
          "Alternative timing \\(scenario-alternative\\)"
   OR NOT EXISTS "${generate_legacy_dir}")
    message(FATAL_ERROR
        "wave-generate legacy Scenario-ID compatibility failed: "
        "${generate_legacy_output}${generate_legacy_error}")
endif()

execute_process(
    COMMAND "${WAVE_GENERATE}" "${duplicate_id_project}"
            "${generate_duplicate_dir}"
            "--scenario=scenario-handshake"
    RESULT_VARIABLE generate_duplicate_code
    OUTPUT_VARIABLE generate_duplicate_output
    ERROR_VARIABLE generate_duplicate_error
)
if(NOT generate_duplicate_code EQUAL 2
   OR NOT generate_duplicate_error MATCHES "ambiguous"
   OR EXISTS "${generate_duplicate_dir}")
    message(FATAL_ERROR
        "wave-generate accepted a duplicate Scenario stable ID")
endif()

execute_process(
    COMMAND "${WAVE_BRIDGE}" import-signals
            "${multi_project}" "${missing_signals}"
            "${bridge_implicit_output}"
    RESULT_VARIABLE bridge_implicit_code
    OUTPUT_VARIABLE bridge_implicit_stdout
    ERROR_VARIABLE bridge_implicit_error
)
if(NOT bridge_implicit_code EQUAL 2
   OR NOT bridge_implicit_error MATCHES "contains 2 scenarios"
   OR EXISTS "${bridge_implicit_output}")
    message(FATAL_ERROR
        "wave-bridge import-signals silently selected a Scenario")
endif()

execute_process(
    COMMAND "${WAVE_BRIDGE}" import-signals
            "${multi_project}" "${SIGNALS}"
            "${imported_project}"
            "--scenario=Alternative timing"
    RESULT_VARIABLE bridge_import_code
    OUTPUT_VARIABLE bridge_import_output
    ERROR_VARIABLE bridge_import_error
)
if(NOT bridge_import_code EQUAL 0
   OR NOT bridge_import_output MATCHES
          "Alternative timing \\(scenario-alternative\\)"
   OR NOT EXISTS "${imported_project}")
    message(FATAL_ERROR
        "wave-bridge import-signals selection failed: "
        "${bridge_import_output}${bridge_import_error}")
endif()
file(READ "${imported_project}" imported_json)
string(JSON first_lane_count
       LENGTH "${imported_json}" scenarios 0 lanes)
string(JSON second_lane_count
       LENGTH "${imported_json}" scenarios 1 lanes)
math(EXPR expected_second_lane_count
     "${first_lane_count} + 1")
if(NOT second_lane_count EQUAL expected_second_lane_count)
    message(FATAL_ERROR
        "Signal import did not modify exactly the selected Scenario")
endif()
set(first_has_ready FALSE)
math(EXPR first_lane_last "${first_lane_count} - 1")
foreach(lane_index RANGE 0 ${first_lane_last})
    string(JSON lane_id
           GET "${imported_json}" scenarios 0 lanes
               ${lane_index} id)
    if(lane_id STREQUAL "zeroslack-ready")
        set(first_has_ready TRUE)
    endif()
endforeach()
set(second_has_ready FALSE)
math(EXPR second_lane_last "${second_lane_count} - 1")
foreach(lane_index RANGE 0 ${second_lane_last})
    string(JSON lane_id
           GET "${imported_json}" scenarios 1 lanes
               ${lane_index} id)
    if(lane_id STREQUAL "zeroslack-ready")
        set(second_has_ready TRUE)
    endif()
endforeach()
if(first_has_ready OR NOT second_has_ready)
    message(FATAL_ERROR
        "Signal import leaked into the unselected Scenario")
endif()

execute_process(
    COMMAND "${WAVE_BRIDGE}" pinloom-entry
            "${multi_project}" "${missing_artifact_dir}"
            "${pinloom_implicit_output}"
    RESULT_VARIABLE pinloom_implicit_code
    OUTPUT_VARIABLE pinloom_implicit_stdout
    ERROR_VARIABLE pinloom_implicit_error
)
if(NOT pinloom_implicit_code EQUAL 2
   OR NOT pinloom_implicit_error MATCHES "contains 2 scenarios"
   OR EXISTS "${pinloom_implicit_output}")
    message(FATAL_ERROR
        "wave-bridge pinloom-entry silently selected a Scenario")
endif()

execute_process(
    COMMAND "${WAVE_BRIDGE}" pinloom-entry
            "${multi_project}" "${ARTIFACT_DIR}"
            "${pinloom_output}"
            "--scenario=scenario-alternative"
    RESULT_VARIABLE pinloom_code
    OUTPUT_VARIABLE pinloom_stdout
    ERROR_VARIABLE pinloom_error
)
if(NOT pinloom_code EQUAL 0
   OR NOT pinloom_stdout MATCHES
          "Alternative timing \\(scenario-alternative\\)"
   OR NOT EXISTS "${pinloom_output}")
    message(FATAL_ERROR
        "wave-bridge pinloom-entry selection failed: "
        "${pinloom_stdout}${pinloom_error}")
endif()
file(READ "${pinloom_output}" pinloom_json)
string(JSON pinloom_scenario_id
       GET "${pinloom_json}" scenarioId)
if(NOT pinloom_scenario_id STREQUAL "scenario-alternative")
    message(FATAL_ERROR
        "Pinloom entry used the wrong Scenario")
endif()

file(WRITE "${lifecycle_operations}" [=[
{
  "schema": "wave-workbench.operations/v1",
  "scenarioId": "scenario-alternative",
  "operations": [
    {"op": "duplicate-scenario", "id": "scenario-cli-copy"},
    {"op": "rename-scenario", "name": "CLI lifecycle copy"},
    {"op": "reorder-scenario", "destinationIndex": 0},
    {"op": "create-scenario", "id": "scenario-cli-created", "name": "CLI created", "durationTick": 123456},
    {"op": "delete-scenario"}
  ]
}
]=])
file(SHA256 "${multi_project}" lifecycle_source_sha)
execute_process(
    COMMAND "${WAVE_CLI}" apply
            "${multi_project}" "${lifecycle_operations}"
            --dry-run --pretty
    RESULT_VARIABLE lifecycle_dry_code
    OUTPUT_VARIABLE lifecycle_dry_json
    ERROR_VARIABLE lifecycle_dry_error
)
if(NOT lifecycle_dry_code EQUAL 0
   OR EXISTS "${lifecycle_result}")
    message(FATAL_ERROR
        "Scenario lifecycle dry-run failed: ${lifecycle_dry_error}")
endif()
string(JSON lifecycle_dry_source_sha
       GET "${lifecycle_dry_json}" sourceSha256)
string(JSON lifecycle_dry_count
       GET "${lifecycle_dry_json}" operationCount)
string(JSON lifecycle_dry_written
       GET "${lifecycle_dry_json}" written)
if(NOT lifecycle_dry_source_sha STREQUAL lifecycle_source_sha
   OR NOT lifecycle_dry_count EQUAL 5
   OR lifecycle_dry_written)
    message(FATAL_ERROR
        "Scenario lifecycle dry-run omitted source SHA or operation evidence")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" apply
            "${multi_project}" "${lifecycle_operations}"
            "--output=${lifecycle_result}" --pretty
    RESULT_VARIABLE lifecycle_code
    OUTPUT_VARIABLE lifecycle_json
    ERROR_VARIABLE lifecycle_error
)
if(NOT lifecycle_code EQUAL 0
   OR NOT EXISTS "${lifecycle_result}")
    message(FATAL_ERROR
        "Scenario lifecycle apply failed: ${lifecycle_error}")
endif()
file(READ "${lifecycle_result}" lifecycle_project_json)
string(JSON lifecycle_scenario_count
       LENGTH "${lifecycle_project_json}" scenarios)
string(JSON lifecycle_first_id
       GET "${lifecycle_project_json}" scenarios 0 id)
string(JSON lifecycle_first_name
       GET "${lifecycle_project_json}" scenarios 0 name)
string(JSON lifecycle_report_scenario
       GET "${lifecycle_json}" scenarioId)
string(JSON lifecycle_created_index
       ERROR_VARIABLE lifecycle_created_error
       GET "${lifecycle_project_json}" scenarios 1 id)
if(NOT lifecycle_scenario_count EQUAL 3
   OR NOT lifecycle_first_id STREQUAL "scenario-cli-copy"
   OR NOT lifecycle_first_name STREQUAL "CLI lifecycle copy"
   OR NOT lifecycle_report_scenario STREQUAL "scenario-handshake")
    message(FATAL_ERROR
        "Scenario lifecycle result did not preserve deterministic selection and order")
endif()
string(FIND "${lifecycle_project_json}" "scenario-cli-created" lifecycle_created_position)
if(NOT lifecycle_created_position EQUAL -1)
    message(FATAL_ERROR
        "Deleted Scenario stable ID remained in the lifecycle output")
endif()

file(WRITE "${lifecycle_invalid_operations}" [=[
{
  "schema": "wave-workbench.operations/v1",
  "scenarioId": "scenario-alternative",
  "operations": [
    {"op": "duplicate-scenario", "id": "scenario-partial"},
    {"op": "delete-scenario", "unexpected": true}
  ]
}
]=])
execute_process(
    COMMAND "${WAVE_CLI}" apply
            "${multi_project}" "${lifecycle_invalid_operations}"
            "--output=${lifecycle_invalid_result}" --pretty
    RESULT_VARIABLE lifecycle_invalid_code
    OUTPUT_VARIABLE lifecycle_invalid_stdout
    ERROR_VARIABLE lifecycle_invalid_json
)
if(NOT lifecycle_invalid_code EQUAL 4
   OR EXISTS "${lifecycle_invalid_result}")
    message(FATAL_ERROR
        "Rejected Scenario lifecycle batch produced a partial output")
endif()
string(JSON lifecycle_failed_operation
       GET "${lifecycle_invalid_json}" error operation)
if(NOT lifecycle_failed_operation EQUAL 1)
    message(FATAL_ERROR
        "Scenario lifecycle diagnostics omitted the failing operation index")
endif()

file(WRITE "${lifecycle_invalid_operations}" [=[
{
  "schema": "wave-workbench.operations/v1",
  "operations": [{"op": "delete-scenario"}]
}
]=])
execute_process(
    COMMAND "${WAVE_CLI}" apply
            "${PROJECT}" "${lifecycle_invalid_operations}"
            "--output=${lifecycle_last_result}" --pretty
    RESULT_VARIABLE lifecycle_last_code
    OUTPUT_VARIABLE lifecycle_last_stdout
    ERROR_VARIABLE lifecycle_last_json
)
if(NOT lifecycle_last_code EQUAL 4
   OR EXISTS "${lifecycle_last_result}"
   OR NOT lifecycle_last_json MATCHES "last Scenario")
    message(FATAL_ERROR
        "wave-cli did not protect the last Scenario atomically")
endif()

file(SHA256 "${multi_project}" lifecycle_source_sha_after)
if(NOT lifecycle_source_sha STREQUAL lifecycle_source_sha_after)
    message(FATAL_ERROR
        "Scenario lifecycle apply modified its source without --in-place")
endif()

file(SHA256 "${PROJECT}" source_sha_after)
if(NOT source_sha_before STREQUAL source_sha_after)
    message(FATAL_ERROR
        "Scenario selection contract modified the source project")
endif()

message(STATUS
    "Scenario selection CLI contract passed; source SHA256=${source_sha_after}")

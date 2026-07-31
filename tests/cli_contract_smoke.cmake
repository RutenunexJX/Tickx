if(NOT DEFINED WAVE_CLI OR NOT DEFINED PROJECT OR NOT DEFINED OPERATIONS OR NOT DEFINED OUTPUT)
    message(FATAL_ERROR "CLI contract smoke requires WAVE_CLI, PROJECT, OPERATIONS, and OUTPUT")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" inspect "${PROJECT}" --scenario=scenario-handshake
    RESULT_VARIABLE inspect_code
    OUTPUT_VARIABLE inspect_json
    ERROR_VARIABLE inspect_error
)
if(NOT inspect_code EQUAL 0)
    message(FATAL_ERROR "inspect failed (${inspect_code}): ${inspect_error}")
endif()
string(JSON inspect_schema GET "${inspect_json}" schema)
string(JSON source_sha GET "${inspect_json}" sourceSha256)
string(JSON duration GET "${inspect_json}" project scenarios 0 durationTick)
string(LENGTH "${source_sha}" source_sha_length)
if(NOT inspect_schema STREQUAL "wave-workbench.cli/v1"
   OR NOT source_sha_length EQUAL 64
   OR NOT source_sha MATCHES "^[0-9a-f]+$"
   OR NOT duration STREQUAL "220000")
    message(FATAL_ERROR "inspect JSON contract is incorrect")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" validate "${PROJECT}" --scenario=scenario-handshake
    RESULT_VARIABLE validate_code
    OUTPUT_VARIABLE validate_json
    ERROR_VARIABLE validate_error
)
if(NOT validate_code EQUAL 0)
    message(FATAL_ERROR "validate failed (${validate_code}): ${validate_error}")
endif()
string(JSON validation_errors GET "${validate_json}" summary errors)
if(NOT validation_errors STREQUAL "0")
    message(FATAL_ERROR "valid fixture reported validation errors")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" validate "${PROJECT}" --scenario=scenario-handshake --fail-on-warning
    RESULT_VARIABLE strict_validate_code
    OUTPUT_VARIABLE strict_validate_json
    ERROR_VARIABLE strict_validate_error
)
if(NOT strict_validate_code EQUAL 4)
    message(FATAL_ERROR "strict warning validation did not return exit code 4")
endif()
string(JSON strict_accepted GET "${strict_validate_json}" accepted)
string(JSON strict_valid GET "${strict_validate_json}" valid)
string(JSON strict_policy GET "${strict_validate_json}" failOnWarning)
if(strict_accepted OR NOT strict_valid OR NOT strict_policy)
    message(FATAL_ERROR "strict warning validation JSON contract is incorrect")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" apply "${PROJECT}" - --dry-run "--expect-sha256=${source_sha}"
    INPUT_FILE "${OPERATIONS}"
    RESULT_VARIABLE dry_run_code
    OUTPUT_VARIABLE dry_run_json
    ERROR_VARIABLE dry_run_error
)
if(NOT dry_run_code EQUAL 0)
    message(FATAL_ERROR "stdin dry-run failed (${dry_run_code}): ${dry_run_error}")
endif()
string(JSON dry_run GET "${dry_run_json}" dryRun)
string(JSON dry_run_written GET "${dry_run_json}" written)
string(JSON operation_count GET "${dry_run_json}" operationCount)
string(JSON dry_run_result_sha GET "${dry_run_json}" resultSha256)
if(NOT dry_run OR dry_run_written OR NOT operation_count EQUAL 4)
    message(FATAL_ERROR "dry-run JSON contract is incorrect")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" apply "${PROJECT}" "${OPERATIONS}" "--output=${OUTPUT}" "--expect-sha256=${source_sha}"
    RESULT_VARIABLE apply_code
    OUTPUT_VARIABLE apply_json
    ERROR_VARIABLE apply_error
)
if(NOT apply_code EQUAL 0)
    message(FATAL_ERROR "atomic apply failed (${apply_code}): ${apply_error}")
endif()
string(JSON written GET "${apply_json}" written)
string(JSON result_sha GET "${apply_json}" resultSha256)
string(LENGTH "${result_sha}" result_sha_length)
if(NOT written
   OR NOT result_sha_length EQUAL 64
   OR NOT result_sha MATCHES "^[0-9a-f]+$"
   OR NOT result_sha STREQUAL "${dry_run_result_sha}"
   OR NOT EXISTS "${OUTPUT}")
    message(FATAL_ERROR "atomic apply did not match dry-run or create its output")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" inspect "${OUTPUT}" --scenario=scenario-handshake
    RESULT_VARIABLE output_inspect_code
    OUTPUT_VARIABLE output_inspect_json
    ERROR_VARIABLE output_inspect_error
)
if(NOT output_inspect_code EQUAL 0)
    message(FATAL_ERROR "applied project inspect failed: ${output_inspect_error}")
endif()
string(JSON output_duration GET "${output_inspect_json}" project scenarios 0 durationTick)
string(JSON renamed_lane GET "${output_inspect_json}" project scenarios 0 lanes 3 name)
string(JSON added_lane GET "${output_inspect_json}" project scenarios 0 lanes 8 name)
string(JSON added_value GET "${output_inspect_json}" project scenarios 0 lanes 8 segments 0 value)
if(NOT output_duration STREQUAL "230000"
   OR NOT renamed_lane STREQUAL "req_cli"
   OR NOT added_lane STREQUAL "cli_data"
   OR NOT added_value STREQUAL "0xa5")
    message(FATAL_ERROR "applied project contents are incorrect")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" apply "${PROJECT}" "${OPERATIONS}" --dry-run "--expect-sha256=0000000000000000000000000000000000000000000000000000000000000000"
    RESULT_VARIABLE conflict_code
    OUTPUT_VARIABLE conflict_output
    ERROR_VARIABLE conflict_json
)
if(NOT conflict_code EQUAL 4)
    message(FATAL_ERROR "stale source hash did not return exit code 4")
endif()
string(JSON conflict_error GET "${conflict_json}" error code)
if(NOT conflict_error STREQUAL "source-conflict")
    message(FATAL_ERROR "stale source hash did not return source-conflict JSON")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" inspect "${PROJECT}" --scenario=scenario-handshake
    RESULT_VARIABLE final_inspect_code
    OUTPUT_VARIABLE final_inspect_json
    ERROR_VARIABLE final_inspect_error
)
if(NOT final_inspect_code EQUAL 0)
    message(FATAL_ERROR "final source inspect failed: ${final_inspect_error}")
endif()
string(JSON final_source_sha GET "${final_inspect_json}" sourceSha256)
if(NOT final_source_sha STREQUAL "${source_sha}")
    message(FATAL_ERROR "CLI apply modified the source fixture")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" apply "${PROJECT}" "${OPERATIONS}" "--output=${PROJECT}"
    RESULT_VARIABLE ambiguous_output_code
    OUTPUT_VARIABLE ambiguous_output
    ERROR_VARIABLE ambiguous_output_json
)
if(NOT ambiguous_output_code EQUAL 2)
    message(FATAL_ERROR "--output=source did not require explicit in-place mode")
endif()
string(JSON ambiguous_output_error GET "${ambiguous_output_json}" error code)
if(NOT ambiguous_output_error STREQUAL "usage")
    message(FATAL_ERROR "--output=source did not return a usage error")
endif()

set(inplace_project "${OUTPUT}.in-place.wave.json")
configure_file("${PROJECT}" "${inplace_project}" COPYONLY)
execute_process(
    COMMAND "${WAVE_CLI}" inspect "${inplace_project}" --scenario=scenario-handshake
    RESULT_VARIABLE inplace_before_code
    OUTPUT_VARIABLE inplace_before_json
    ERROR_VARIABLE inplace_before_error
)
if(NOT inplace_before_code EQUAL 0)
    message(FATAL_ERROR "in-place source inspect failed: ${inplace_before_error}")
endif()
string(JSON inplace_before_sha GET "${inplace_before_json}" sourceSha256)
execute_process(
    COMMAND "${WAVE_CLI}" apply "${inplace_project}" "${OPERATIONS}"
            --in-place --backup "--expect-sha256=${inplace_before_sha}"
    RESULT_VARIABLE inplace_code
    OUTPUT_VARIABLE inplace_json
    ERROR_VARIABLE inplace_error
)
if(NOT inplace_code EQUAL 0)
    message(FATAL_ERROR "in-place apply failed (${inplace_code}): ${inplace_error}")
endif()
string(JSON inplace_written GET "${inplace_json}" written)
string(JSON inplace_path GET "${inplace_json}" outputPath)
string(JSON backup_available GET "${inplace_json}" backupAvailable)
string(JSON backup_path GET "${inplace_json}" backupPath)
if(NOT inplace_written
   OR NOT inplace_path STREQUAL "${inplace_project}"
   OR NOT backup_available
   OR NOT backup_path MATCHES "\\.backup-[0-9a-f]+\\.wave\\.json$"
   OR NOT EXISTS "${backup_path}")
    message(FATAL_ERROR "in-place apply did not report the replaced file")
endif()
execute_process(
    COMMAND "${WAVE_CLI}" inspect "${backup_path}" --scenario=scenario-handshake
    RESULT_VARIABLE backup_code
    OUTPUT_VARIABLE backup_json
    ERROR_VARIABLE backup_error
)
if(NOT backup_code EQUAL 0)
    message(FATAL_ERROR "in-place source backup is unreadable: ${backup_error}")
endif()
string(JSON backup_sha GET "${backup_json}" sourceSha256)
string(JSON backup_duration GET "${backup_json}" project scenarios 0 durationTick)
if(NOT backup_sha STREQUAL "${inplace_before_sha}"
   OR NOT backup_duration STREQUAL "220000")
    message(FATAL_ERROR "in-place backup did not preserve the original source")
endif()
execute_process(
    COMMAND "${WAVE_CLI}" inspect "${inplace_project}" --scenario=scenario-handshake
    RESULT_VARIABLE inplace_after_code
    OUTPUT_VARIABLE inplace_after_json
    ERROR_VARIABLE inplace_after_error
)
if(NOT inplace_after_code EQUAL 0)
    message(FATAL_ERROR "in-place result inspect failed: ${inplace_after_error}")
endif()
string(JSON inplace_after_sha GET "${inplace_after_json}" sourceSha256)
string(JSON inplace_duration GET "${inplace_after_json}" project scenarios 0 durationTick)
if(inplace_after_sha STREQUAL "${inplace_before_sha}"
   OR NOT inplace_duration STREQUAL "230000")
    message(FATAL_ERROR "in-place apply did not replace the project atomically")
endif()

set(conflict_inplace_project "${OUTPUT}.backup-conflict.wave.json")
configure_file("${PROJECT}" "${conflict_inplace_project}" COPYONLY)
execute_process(
    COMMAND "${WAVE_CLI}" apply "${conflict_inplace_project}" "${OPERATIONS}"
            --in-place "--backup=${OUTPUT}"
    RESULT_VARIABLE backup_conflict_code
    OUTPUT_VARIABLE backup_conflict_output
    ERROR_VARIABLE backup_conflict_json
)
if(NOT backup_conflict_code EQUAL 4)
    message(FATAL_ERROR "different existing backup did not reject in-place write")
endif()
string(JSON backup_conflict_error GET "${backup_conflict_json}" error code)
if(NOT backup_conflict_error STREQUAL "backup-conflict")
    message(FATAL_ERROR "different existing backup did not report backup-conflict")
endif()
execute_process(
    COMMAND "${WAVE_CLI}" inspect "${conflict_inplace_project}" --summary
    RESULT_VARIABLE conflict_source_code
    OUTPUT_VARIABLE conflict_source_json
    ERROR_VARIABLE conflict_source_error
)
if(NOT conflict_source_code EQUAL 0)
    message(FATAL_ERROR "backup-conflict source became unreadable: ${conflict_source_error}")
endif()
string(JSON conflict_source_sha GET "${conflict_source_json}" sourceSha256)
if(NOT conflict_source_sha STREQUAL "${source_sha}")
    message(FATAL_ERROR "backup-conflict modified the in-place source")
endif()

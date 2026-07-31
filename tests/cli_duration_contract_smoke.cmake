if(NOT DEFINED WAVE_CLI
   OR NOT DEFINED PROJECT
   OR NOT DEFINED TRUNCATE_OPERATIONS
   OR NOT DEFINED GUARDED_OPERATIONS
   OR NOT DEFINED SAFE_OPERATIONS
   OR NOT DEFINED OUTPUT)
    message(FATAL_ERROR "Duration CLI contract smoke is missing an input")
endif()

set(result_output "${OUTPUT}.wave.json")
set(guarded_output "${OUTPUT}-guarded.wave.json")
set(safe_output "${OUTPUT}-safe.wave.json")
file(REMOVE "${result_output}" "${guarded_output}" "${safe_output}")

execute_process(
    COMMAND "${WAVE_CLI}" capabilities
    RESULT_VARIABLE capabilities_code
    OUTPUT_VARIABLE capabilities_json
    ERROR_VARIABLE capabilities_error
)
if(NOT capabilities_code EQUAL 0)
    message(FATAL_ERROR "duration capability discovery failed: ${capabilities_error}")
endif()
string(JSON supports_extension
       GET "${capabilities_json}" scenarioDuration supportsExtension)
string(JSON supports_safe_shrink
       GET "${capabilities_json}" scenarioDuration supportsSafeShrink)
string(JSON truncation_field
       GET "${capabilities_json}" scenarioDuration destructiveShrinkOptInField)
string(JSON reports_counts
       GET "${capabilities_json}" scenarioDuration reportsTruncationCounts)
if(NOT supports_extension
   OR NOT supports_safe_shrink
   OR NOT truncation_field STREQUAL "truncate"
   OR NOT reports_counts)
    message(FATAL_ERROR "capabilities omit the duration resize safety contract")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" inspect "${PROJECT}" --summary
    RESULT_VARIABLE source_code
    OUTPUT_VARIABLE source_json
    ERROR_VARIABLE source_error
)
if(NOT source_code EQUAL 0)
    message(FATAL_ERROR "duration source inspection failed: ${source_error}")
endif()
string(JSON source_sha GET "${source_json}" sourceSha256)
string(JSON source_duration
       GET "${source_json}" project scenarios 0 durationTick)
if(NOT source_duration STREQUAL "220000")
    message(FATAL_ERROR "duration source fixture has an unexpected End")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" apply "${PROJECT}" "${GUARDED_OPERATIONS}"
            "--output=${guarded_output}"
    RESULT_VARIABLE guarded_code
    OUTPUT_VARIABLE guarded_stdout
    ERROR_VARIABLE guarded_json
)
if(NOT guarded_code EQUAL 4
   OR NOT guarded_stdout STREQUAL ""
   OR EXISTS "${guarded_output}")
    message(FATAL_ERROR "unconfirmed destructive shrink was accepted or wrote output")
endif()
string(JSON guarded_error_code GET "${guarded_json}" error code)
string(JSON guarded_operation GET "${guarded_json}" error operation)
string(JSON guarded_message GET "${guarded_json}" error message)
if(NOT guarded_error_code STREQUAL "operation-rejected"
   OR NOT guarded_operation EQUAL 1
   OR NOT guarded_message MATCHES "truncate"
   OR NOT guarded_message MATCHES "clip")
    message(FATAL_ERROR "duration guard did not report actionable content-loss details")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" apply "${PROJECT}" "${TRUNCATE_OPERATIONS}"
            "--output=${result_output}" --dry-run
    RESULT_VARIABLE dry_code
    OUTPUT_VARIABLE dry_json
    ERROR_VARIABLE dry_error
)
if(NOT dry_code EQUAL 0 OR EXISTS "${result_output}")
    message(FATAL_ERROR "duration truncation dry-run failed: ${dry_error}")
endif()
string(JSON dry_sha GET "${dry_json}" resultSha256)
string(JSON dry_changed GET "${dry_json}" operations 0 changed)
string(JSON dry_direction GET "${dry_json}" operations 0 direction)
string(JSON dry_requested GET "${dry_json}" operations 0 truncateRequested)
string(JSON dry_truncated GET "${dry_json}" operations 0 contentTruncated)
string(JSON dry_clipped_segments
       GET "${dry_json}" operations 0 clippedSegmentCount)
string(JSON dry_removed_segments
       GET "${dry_json}" operations 0 removedSegmentCount)
string(JSON dry_removed_events
       GET "${dry_json}" operations 0 removedEventCount)
string(JSON dry_removed_relations
       GET "${dry_json}" operations 0 removedRelationCount)
string(JSON dry_clipped_markers
       GET "${dry_json}" operations 0 clippedMarkerCount)
string(JSON dry_removed_markers
       GET "${dry_json}" operations 0 removedMarkerCount)
string(JSON dry_duration
       GET "${dry_json}" changes scenarios 0 afterDurationTick)
string(JSON dry_segment_count
       GET "${dry_json}" changes scenarios 0 afterSegmentCount)
string(JSON dry_event_count
       GET "${dry_json}" changes scenarios 0 afterEventCount)
if(NOT dry_changed
   OR NOT dry_direction STREQUAL "shrink"
   OR NOT dry_requested
   OR NOT dry_truncated
   OR NOT dry_clipped_segments EQUAL 6
   OR NOT dry_removed_segments EQUAL 6
   OR NOT dry_removed_events EQUAL 4
   OR NOT dry_removed_relations EQUAL 0
   OR NOT dry_clipped_markers EQUAL 1
   OR NOT dry_removed_markers EQUAL 0
   OR NOT dry_duration STREQUAL "120000"
   OR NOT dry_segment_count EQUAL 11
   OR NOT dry_event_count EQUAL 10)
    message(FATAL_ERROR "duration truncation dry-run report is incomplete")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" apply "${PROJECT}" "${TRUNCATE_OPERATIONS}"
            "--output=${result_output}"
    RESULT_VARIABLE write_code
    OUTPUT_VARIABLE write_json
    ERROR_VARIABLE write_error
)
if(NOT write_code EQUAL 0 OR NOT EXISTS "${result_output}")
    message(FATAL_ERROR "duration truncation write failed: ${write_error}")
endif()
string(JSON write_sha GET "${write_json}" resultSha256)
if(NOT write_sha STREQUAL "${dry_sha}")
    message(FATAL_ERROR "duration truncation dry-run and write SHA differ")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" inspect "${result_output}" --summary
    RESULT_VARIABLE inspect_code
    OUTPUT_VARIABLE inspect_json
    ERROR_VARIABLE inspect_error
)
if(NOT inspect_code EQUAL 0)
    message(FATAL_ERROR "truncated project inspection failed: ${inspect_error}")
endif()
string(JSON result_duration
       GET "${inspect_json}" project scenarios 0 durationTick)
string(JSON result_events
       GET "${inspect_json}" project scenarios 0 eventCount)
string(JSON result_relations
       GET "${inspect_json}" project scenarios 0 relationCount)
string(JSON result_markers
       GET "${inspect_json}" project scenarios 0 markerCount)
if(NOT result_duration STREQUAL "120000"
   OR NOT result_events EQUAL 10
   OR NOT result_relations EQUAL 1
   OR NOT result_markers EQUAL 1)
    message(FATAL_ERROR "truncated project retained content beyond End")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" sample "${result_output}" "--at=119 ns"
            --lane=req --lane=ack "--lane=data[7:0]" --lane=state
    RESULT_VARIABLE sample_code
    OUTPUT_VARIABLE sample_json
    ERROR_VARIABLE sample_error
)
if(NOT sample_code EQUAL 0)
    message(FATAL_ERROR "sampling the truncated boundary failed: ${sample_error}")
endif()
string(JSON req_value GET "${sample_json}" samples 0 value)
string(JSON ack_value GET "${sample_json}" samples 1 value)
string(JSON data_value GET "${sample_json}" samples 2 value)
string(JSON state_value GET "${sample_json}" samples 3 value)
if(NOT req_value STREQUAL "1"
   OR NOT ack_value STREQUAL "1"
   OR NOT data_value STREQUAL "0x35"
   OR NOT state_value STREQUAL "WAIT_ACK")
    message(FATAL_ERROR "duration truncation changed values before the new End")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" validate "${result_output}"
    RESULT_VARIABLE validate_code
    OUTPUT_VARIABLE validate_json
    ERROR_VARIABLE validate_error
)
if(NOT validate_code EQUAL 0)
    message(FATAL_ERROR "truncated project validation failed: ${validate_error}")
endif()
string(JSON validation_errors GET "${validate_json}" summary errors)
if(NOT validation_errors STREQUAL "0")
    message(FATAL_ERROR "duration truncation introduced validation errors")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" new "${safe_output}"
            "--name=Safe duration shrink"
            "--duration=60 ns"
            "--operations=${SAFE_OPERATIONS}"
            --dry-run
    RESULT_VARIABLE safe_dry_code
    OUTPUT_VARIABLE safe_dry_json
    ERROR_VARIABLE safe_dry_error
)
if(NOT safe_dry_code EQUAL 0 OR EXISTS "${safe_output}")
    message(FATAL_ERROR "safe empty-tail shrink dry-run failed: ${safe_dry_error}")
endif()
string(JSON safe_sha GET "${safe_dry_json}" resultSha256)
string(JSON safe_duration
       GET "${safe_dry_json}" project scenarios 0 durationTick)
string(JSON safe_direction
       GET "${safe_dry_json}" operations 0 direction)
string(JSON safe_requested
       GET "${safe_dry_json}" operations 0 truncateRequested)
string(JSON safe_truncated
       GET "${safe_dry_json}" operations 0 contentTruncated)
string(JSON safe_clipped
       GET "${safe_dry_json}" operations 0 clippedSegmentCount)
string(JSON safe_removed
       GET "${safe_dry_json}" operations 0 removedEventCount)
if(NOT safe_duration STREQUAL "40000"
   OR NOT safe_direction STREQUAL "shrink"
   OR safe_requested
   OR safe_truncated
   OR NOT safe_clipped EQUAL 0
   OR NOT safe_removed EQUAL 0)
    message(FATAL_ERROR "safe empty-tail shrink required destructive opt-in")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" new "${safe_output}"
            "--name=Safe duration shrink"
            "--duration=60 ns"
            "--operations=${SAFE_OPERATIONS}"
    RESULT_VARIABLE safe_write_code
    OUTPUT_VARIABLE safe_write_json
    ERROR_VARIABLE safe_write_error
)
if(NOT safe_write_code EQUAL 0 OR NOT EXISTS "${safe_output}")
    message(FATAL_ERROR "safe empty-tail shrink write failed: ${safe_write_error}")
endif()
string(JSON safe_write_sha GET "${safe_write_json}" resultSha256)
if(NOT safe_write_sha STREQUAL "${safe_sha}")
    message(FATAL_ERROR "safe shrink dry-run and write SHA differ")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" inspect "${PROJECT}" --summary
    RESULT_VARIABLE source_after_code
    OUTPUT_VARIABLE source_after_json
    ERROR_VARIABLE source_after_error
)
if(NOT source_after_code EQUAL 0)
    message(FATAL_ERROR "duration source reinspection failed: ${source_after_error}")
endif()
string(JSON source_after_sha GET "${source_after_json}" sourceSha256)
if(NOT source_after_sha STREQUAL "${source_sha}")
    message(FATAL_ERROR "duration operations changed their source project")
endif()

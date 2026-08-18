if(NOT DEFINED WAVE_SIM_RUNNER OR NOT DEFINED VERILATOR_FIXTURE
   OR NOT DEFINED CXX_FIXTURE OR NOT DEFINED SIMULATOR_FIXTURE
   OR NOT DEFINED FIXTURE_ROOT OR NOT DEFINED OUTPUT)
    message(FATAL_ERROR "fixed fixture simulation smoke is missing arguments")
endif()

file(REMOVE_RECURSE "${OUTPUT}")
file(MAKE_DIRECTORY "${OUTPUT}")
set(session_root "${OUTPUT}/session-input")
file(COPY "${FIXTURE_ROOT}/" DESTINATION "${session_root}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
            "WAVE_SIMULATOR_FIXTURE=${SIMULATOR_FIXTURE}"
            "WAVE_VERILATOR_FIXTURE_COUNT_FILE=${OUTPUT}/build-count.txt"
            "${WAVE_SIM_RUNNER}" run-module
            "--manifest=${session_root}/manifest.json"
            "--stimulus=${session_root}/stimulus.json"
            "--workspace=${session_root}"
            "--artifacts=${OUTPUT}"
            "--build-cache=${OUTPUT}/build-cache"
            "--scenario-directory=${OUTPUT}/scenarios/fixed-counter"
            "--result-project=${OUTPUT}/result.wave.json"
            "--verilator=${VERILATOR_FIXTURE}"
            "--cxx=${CXX_FIXTURE}"
            --probe-timeout-ms=1000
            --build-timeout-ms=2000
            --run-timeout-ms=2000
    RESULT_VARIABLE result
    OUTPUT_VARIABLE report
    ERROR_VARIABLE error
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "fixed fixture runner failed (${result}): ${error}\n${report}")
endif()
string(JSON schema GET "${report}" schema)
string(JSON status GET "${report}" status)
string(JSON signal_count GET "${report}" trace signalCount)
string(JSON transition_count GET "${report}" trace transitionCount)
string(JSON harness GET "${report}" artifacts harness)
string(JSON simulator GET "${report}" artifacts executable)
string(JSON vcd GET "${report}" artifacts vcd)
string(JSON result_project GET "${report}" artifacts resultProject)
string(JSON cache_hit GET "${report}" buildCache hit)
string(JSON cache_published GET "${report}" buildCache published)
if(NOT schema STREQUAL "wave-workbench.simulation-run/v1"
   OR NOT status STREQUAL "succeeded"
   OR signal_count LESS 4 OR transition_count LESS 12
   OR NOT EXISTS "${harness}" OR NOT EXISTS "${simulator}" OR NOT EXISTS "${vcd}"
   OR NOT EXISTS "${result_project}"
   OR cache_hit OR NOT cache_published)
    message(FATAL_ERROR "fixed fixture report or artifacts are incomplete: ${report}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
            "WAVE_SIMULATOR_FIXTURE=${SIMULATOR_FIXTURE}"
            "WAVE_VERILATOR_FIXTURE_COUNT_FILE=${OUTPUT}/build-count.txt"
            "${WAVE_SIM_RUNNER}" run-module
            "--manifest=${session_root}/manifest.json"
            "--stimulus=${session_root}/stimulus.json"
            "--workspace=${session_root}"
            "--artifacts=${OUTPUT}"
            "--build-cache=${OUTPUT}/build-cache"
            "--scenario-directory=${OUTPUT}/scenarios/fixed-counter"
            "--result-project=${OUTPUT}/result.wave.json"
            "--verilator=${VERILATOR_FIXTURE}"
            "--cxx=${CXX_FIXTURE}"
            --probe-timeout-ms=1000
            --build-timeout-ms=2000
            --run-timeout-ms=2000
    RESULT_VARIABLE rerun_result
    OUTPUT_VARIABLE rerun_report
    ERROR_VARIABLE rerun_error
)
if(NOT rerun_result EQUAL 0)
    message(FATAL_ERROR
        "stimulus-only fixed fixture rerun failed (${rerun_result}): ${rerun_error}\n${rerun_report}")
endif()
string(JSON rerun_status GET "${rerun_report}" status)
string(JSON rerun_cache_hit GET "${rerun_report}" buildCache hit)
string(JSON rerun_fingerprint GET "${rerun_report}" buildCache fingerprint)
string(JSON first_fingerprint GET "${report}" buildCache fingerprint)
string(JSON rerun_build_type ERROR_VARIABLE rerun_build_error
       TYPE "${rerun_report}" build)
file(STRINGS "${OUTPUT}/build-count.txt" build_count_lines)
list(LENGTH build_count_lines build_count)
if(NOT rerun_status STREQUAL "succeeded"
   OR NOT rerun_cache_hit
   OR NOT rerun_fingerprint STREQUAL first_fingerprint
   OR NOT rerun_build_error
   OR NOT build_count EQUAL 1)
    message(FATAL_ERROR
        "stimulus-only CLI rerun did not reuse exactly one verified model: ${rerun_report}")
endif()
file(READ "${result_project}" result_document)
string(JSON imported_trace_count LENGTH "${result_document}" importedTraces)
string(JSON imported_trace_path GET "${result_document}" importedTraces 0 path)
if(NOT imported_trace_count EQUAL 1 OR imported_trace_path STREQUAL "")
    message(FATAL_ERROR "result project did not retain its imported trace: ${result_document}")
endif()

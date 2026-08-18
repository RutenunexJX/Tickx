if(NOT DEFINED WAVE_SIM_RUNNER OR NOT DEFINED VERILATOR_FIXTURE
   OR NOT DEFINED CXX_FIXTURE OR NOT DEFINED SIMULATOR_FIXTURE
   OR NOT DEFINED FIXTURE_ROOT OR NOT DEFINED OUTPUT)
    message(FATAL_ERROR "fixed fixture simulation smoke is missing arguments")
endif()

file(REMOVE_RECURSE "${OUTPUT}")
file(MAKE_DIRECTORY "${OUTPUT}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
            "WAVE_SIMULATOR_FIXTURE=${SIMULATOR_FIXTURE}"
            "${WAVE_SIM_RUNNER}" run-module
            "--manifest=${FIXTURE_ROOT}/manifest.json"
            "--stimulus=${FIXTURE_ROOT}/stimulus.json"
            "--workspace=${FIXTURE_ROOT}"
            "--artifacts=${OUTPUT}"
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
if(NOT schema STREQUAL "wave-workbench.simulation-run/v1"
   OR NOT status STREQUAL "succeeded"
   OR signal_count LESS 4 OR transition_count LESS 12
   OR NOT EXISTS "${harness}" OR NOT EXISTS "${simulator}" OR NOT EXISTS "${vcd}"
   OR NOT EXISTS "${result_project}")
    message(FATAL_ERROR "fixed fixture report or artifacts are incomplete: ${report}")
endif()
file(READ "${result_project}" result_document)
string(JSON imported_trace_count LENGTH "${result_document}" importedTraces)
string(JSON imported_trace_path GET "${result_document}" importedTraces 0 path)
if(NOT imported_trace_count EQUAL 1 OR imported_trace_path STREQUAL "")
    message(FATAL_ERROR "result project did not retain its imported trace: ${result_document}")
endif()

if(NOT DEFINED WAVE_SIM_RUNNER OR NOT DEFINED VERILATOR
   OR NOT DEFINED CXX OR NOT DEFINED FIXTURE_ROOT OR NOT DEFINED OUTPUT)
    message(FATAL_ERROR "real Verilator fixture smoke is missing arguments")
endif()

file(REMOVE_RECURSE "${OUTPUT}")
file(MAKE_DIRECTORY "${OUTPUT}")
execute_process(
    COMMAND "${WAVE_SIM_RUNNER}" run-fixture
            "--manifest=${FIXTURE_ROOT}/manifest.json"
            "--stimulus=${FIXTURE_ROOT}/stimulus.json"
            "--workspace=${FIXTURE_ROOT}"
            "--artifacts=${OUTPUT}"
            "--verilator=${VERILATOR}"
            "--cxx=${CXX}"
            --probe-timeout-ms=5000
            --build-timeout-ms=120000
            --run-timeout-ms=30000
    RESULT_VARIABLE result
    OUTPUT_VARIABLE report
    ERROR_VARIABLE error
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "real Verilator fixture failed (${result}): ${error}\n${report}")
endif()
string(JSON status GET "${report}" status)
string(JSON signal_count GET "${report}" trace signalCount)
string(JSON transition_count GET "${report}" trace transitionCount)
if(NOT status STREQUAL "succeeded" OR signal_count LESS 4
   OR transition_count LESS 12)
    message(FATAL_ERROR "real Verilator fixture report is incomplete: ${report}")
endif()

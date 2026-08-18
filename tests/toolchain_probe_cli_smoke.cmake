foreach(required IN ITEMS
    WAVE_SIM_RUNNER READY_FIXTURE OLD_FIXTURE FAILURE_FIXTURE HANG_FIXTURE OUTPUT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "Toolchain probe smoke is missing ${required}")
    endif()
endforeach()

file(MAKE_DIRECTORY "${OUTPUT}")
set(missing_fixture "${OUTPUT}/definitely-missing-tool")

function(run_probe name expected_exit expected_status verilator cxx timeout_ms cancel_ms)
    set(arguments
        probe
        "--verilator=${verilator}"
        "--cxx=${cxx}"
        "--timeout-ms=${timeout_ms}"
        --pretty)
    if(NOT cancel_ms STREQUAL "")
        list(APPEND arguments "--cancel-after-ms=${cancel_ms}")
    endif()
    execute_process(
        COMMAND "${WAVE_SIM_RUNNER}" ${arguments}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
        TIMEOUT 5)
    if(NOT "${result}" STREQUAL "${expected_exit}")
        message(FATAL_ERROR
            "${name}: exit ${result}, expected ${expected_exit}\nstdout=${output}\nstderr=${error}")
    endif()
    if(NOT error STREQUAL "")
        message(FATAL_ERROR "${name}: CLI leaked diagnostics to stderr: ${error}")
    endif()
    string(JSON schema GET "${output}" schema)
    string(JSON schema_version GET "${output}" schemaVersion)
    string(JSON status GET "${output}" status)
    if(NOT schema STREQUAL "wave-workbench.toolchain-probe/v1"
       OR NOT schema_version EQUAL 1
       OR NOT status STREQUAL expected_status)
        message(FATAL_ERROR "${name}: malformed probe report: ${output}")
    endif()
    file(WRITE "${OUTPUT}/${name}.json" "${output}")
endfunction()

run_probe(
    ready 0 ready
    "${READY_FIXTURE}" "${READY_FIXTURE}" 1000 "")
run_probe(
    unavailable 3 unavailable
    "${missing_fixture}" "${missing_fixture}" 1000 "")
run_probe(
    incompatible 4 incompatible
    "${OLD_FIXTURE}" "${OLD_FIXTURE}" 1000 "")
run_probe(
    failed 5 failed
    "${FAILURE_FIXTURE}" "${FAILURE_FIXTURE}" 1000 "")
run_probe(
    timed-out 6 timed-out
    "${HANG_FIXTURE}" "${HANG_FIXTURE}" 40 "")
run_probe(
    cancelled 7 cancelled
    "${HANG_FIXTURE}" "${HANG_FIXTURE}" 1000 40)

file(READ "${OUTPUT}/ready.json" ready_report)
string(JSON ready_ok GET "${ready_report}" ok)
string(JSON verilator_status GET "${ready_report}" tools verilator status)
string(JSON verilator_major GET "${ready_report}" tools verilator version major)
string(JSON compiler_status GET "${ready_report}" tools cxx status)
string(JSON compiler_family GET "${ready_report}" tools cxx compilerFamily)
string(JSON compiler_stderr GET "${ready_report}" tools cxx process stderr)
if(NOT ready_ok
   OR NOT verilator_status STREQUAL "ready"
   OR NOT verilator_major EQUAL 5
   OR NOT compiler_status STREQUAL "ready"
   OR NOT compiler_family STREQUAL "gcc"
   OR NOT compiler_stderr MATCHES "fixture-stderr")
    message(FATAL_ERROR "ready probe lost tool or process evidence")
endif()

file(READ "${OUTPUT}/unavailable.json" unavailable_report)
string(JSON missing_state GET
    "${unavailable_report}" tools verilator process state)
if(NOT missing_state STREQUAL "program-not-found")
    message(FATAL_ERROR "missing executable was not classified deterministically")
endif()

file(READ "${OUTPUT}/incompatible.json" incompatible_report)
string(JSON old_status GET
    "${incompatible_report}" tools verilator status)
if(NOT old_status STREQUAL "incompatible-version")
    message(FATAL_ERROR "old Verilator was not classified as incompatible")
endif()

file(READ "${OUTPUT}/failed.json" failed_report)
string(JSON failure_state GET
    "${failed_report}" tools verilator process state)
string(JSON failure_output GET
    "${failed_report}" tools verilator process stderr)
if(NOT failure_state STREQUAL "nonzero-exit"
   OR NOT failure_output MATCHES "fixture-error-before-failure")
    message(FATAL_ERROR "failed probe lost exit or stderr evidence")
endif()

file(READ "${OUTPUT}/timed-out.json" timeout_report)
string(JSON timeout_state GET
    "${timeout_report}" tools verilator process state)
if(NOT timeout_state STREQUAL "timed-out")
    message(FATAL_ERROR "probe timeout did not stop the child process")
endif()

file(READ "${OUTPUT}/cancelled.json" cancelled_report)
string(JSON cancel_state GET
    "${cancelled_report}" tools verilator process state)
if(NOT cancel_state STREQUAL "cancelled")
    message(FATAL_ERROR "probe cancellation did not stop the child process")
endif()

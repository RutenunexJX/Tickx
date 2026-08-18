if(NOT DEFINED WAVE_CLI
   OR NOT DEFINED PROJECT
   OR NOT DEFINED OPERATIONS
   OR NOT DEFINED OUTPUT)
    message(FATAL_ERROR "CLI compatibility dispatch smoke is missing an input")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" capabilities --pretty
    RESULT_VARIABLE capabilities_result
    OUTPUT_VARIABLE capabilities_json
    ERROR_VARIABLE capabilities_error
)
if(NOT capabilities_result EQUAL 0)
    message(FATAL_ERROR
        "wave-cli capabilities failed: ${capabilities_error}")
endif()

string(JSON command_count GET "${capabilities_json}" compatibilityCommandCount)
set(found_generate FALSE)
set(found_compare FALSE)
set(found_bridge FALSE)
math(EXPR command_last "${command_count} - 1")
foreach(command_index RANGE 0 ${command_last})
    string(JSON command_name
        GET "${capabilities_json}" compatibilityCommands ${command_index} name)
    if(command_name STREQUAL "generate"
       OR command_name STREQUAL "compare"
       OR command_name STREQUAL "bridge")
        string(JSON compatibility_adapter
            GET "${capabilities_json}" compatibilityCommands ${command_index}
            compatibilityAdapter)
        string(JSON structured_output
            GET "${capabilities_json}" compatibilityCommands ${command_index}
            structuredOutput)
        if(NOT compatibility_adapter OR structured_output)
            message(FATAL_ERROR
                "${command_name} is not described as a text compatibility adapter")
        endif()
        set(found_${command_name} TRUE)
    endif()
endforeach()
if(NOT found_generate OR NOT found_compare OR NOT found_bridge)
    message(FATAL_ERROR
        "capabilities omitted one or more compatibility adapters")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" generate --help
    RESULT_VARIABLE help_result
    OUTPUT_VARIABLE help_output
    ERROR_VARIABLE help_error
)
if(NOT help_result EQUAL 0
   OR NOT help_output MATCHES "wave-generate")
    message(FATAL_ERROR
        "wave-cli generate --help is not discoverable: ${help_error}")
endif()

file(REMOVE_RECURSE "${OUTPUT}")
file(MAKE_DIRECTORY "${OUTPUT}")

execute_process(
    COMMAND "${WAVE_CLI}" new "${OUTPUT}/quick-start.wave.json"
            "--name=CLI quick start" "--duration=80 ns"
            "--operations=${OPERATIONS}" --dry-run --pretty
    RESULT_VARIABLE quick_start_result
    OUTPUT_VARIABLE quick_start_json
    ERROR_VARIABLE quick_start_error
)
if(NOT quick_start_result EQUAL 0)
    message(FATAL_ERROR
        "Packaged quick-start operation example failed: ${quick_start_error}")
endif()
string(JSON quick_start_ok GET "${quick_start_json}" ok)
string(JSON quick_start_operation_count
    GET "${quick_start_json}" operationCount)
if(NOT quick_start_ok OR NOT quick_start_operation_count EQUAL 4)
    message(FATAL_ERROR
        "Packaged quick-start operation example returned an incomplete report")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" generate "${PROJECT}" "${OUTPUT}/generated"
    RESULT_VARIABLE generate_result
    OUTPUT_VARIABLE generate_output
    ERROR_VARIABLE generate_error
)
if(NOT generate_result EQUAL 0)
    message(FATAL_ERROR
        "wave-cli generate forwarding failed: ${generate_error}")
endif()
file(GLOB generated_artifacts "${OUTPUT}/generated/*")
list(LENGTH generated_artifacts generated_count)
if(NOT generated_count EQUAL 7)
    message(FATAL_ERROR
        "wave-cli generate produced ${generated_count} artifacts instead of 7")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" compare "${PROJECT}" "${OUTPUT}/compare"
    RESULT_VARIABLE compare_result
    OUTPUT_VARIABLE compare_output
    ERROR_VARIABLE compare_error
)
if(NOT compare_result EQUAL 0)
    message(FATAL_ERROR
        "wave-cli compare forwarding failed: ${compare_error}")
endif()
file(GLOB compare_reports "${OUTPUT}/compare/*")
list(LENGTH compare_reports compare_count)
if(NOT compare_count EQUAL 3)
    message(FATAL_ERROR
        "wave-cli compare produced ${compare_count} reports instead of 3")
endif()

execute_process(
    COMMAND "${WAVE_CLI}" bridge describe "${PROJECT}"
            "${OUTPUT}/workspace-manifest.json"
    RESULT_VARIABLE bridge_result
    OUTPUT_VARIABLE bridge_output
    ERROR_VARIABLE bridge_error
)
if(NOT bridge_result EQUAL 0
   OR NOT EXISTS "${OUTPUT}/workspace-manifest.json")
    message(FATAL_ERROR
        "wave-cli bridge forwarding failed: ${bridge_error}")
endif()

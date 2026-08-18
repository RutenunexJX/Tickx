if(NOT DEFINED BUILD_DIRECTORY OR NOT DEFINED OUTPUT)
    message(FATAL_ERROR "Portable install contract smoke is missing an input")
endif()

file(REMOVE_RECURSE "${OUTPUT}")

set(install_command
    "${CMAKE_COMMAND}" --install "${BUILD_DIRECTORY}"
    --prefix "${OUTPUT}" --component Portable)
if(DEFINED CONFIGURATION AND NOT CONFIGURATION STREQUAL "")
    list(APPEND install_command --config "${CONFIGURATION}")
endif()
execute_process(
    COMMAND ${install_command}
    RESULT_VARIABLE install_result
    OUTPUT_VARIABLE install_output
    ERROR_VARIABLE install_error
)
if(NOT install_result EQUAL 0)
    message(FATAL_ERROR
        "Portable component install failed: ${install_error}\n${install_output}")
endif()

if(WIN32)
    set(executable_suffix ".exe")
else()
    set(executable_suffix "")
endif()
foreach(executable IN ITEMS
    wave-workbench wave-cli wave-generate wave-compare wave-bridge)
    if(NOT EXISTS "${OUTPUT}/${executable}${executable_suffix}")
        message(FATAL_ERROR
            "Portable install omitted ${executable}${executable_suffix}")
    endif()
endforeach()

foreach(document IN ITEMS
    PACKAGE-README.txt
    docs/automation-cli.md
    docs/cli-quick-start.md
    docs/integration-contracts.md
    docs/project-format.md
    docs/examples/quick-start.operations.json
    schemas/automation/v1/capabilities.schema.json
    schemas/automation/v1/report.schema.json
    schemas/automation/v1/operation-batch.schema.json
    examples/handshake/project.wave.json)
    if(NOT EXISTS "${OUTPUT}/${document}")
        message(FATAL_ERROR "Portable install omitted ${document}")
    endif()
endforeach()

file(READ "${OUTPUT}/PACKAGE-README.txt" package_readme)
if(NOT package_readme MATCHES "wave-cli")
    message(FATAL_ERROR "Portable package README does not advertise wave-cli")
endif()

file(READ
    "${OUTPUT}/docs/examples/quick-start.operations.json"
    operations_json)
string(JSON operations_schema GET "${operations_json}" schema)
if(NOT operations_schema STREQUAL "wave-workbench.operations/v1")
    message(FATAL_ERROR
        "Portable quick-start operation example has the wrong schema")
endif()

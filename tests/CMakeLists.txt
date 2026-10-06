function(wave_add_toolchain_fixture target mode)
    add_executable(${target} process_fixture_main.cpp)
    target_link_libraries(${target} PRIVATE Qt6::Core)
    target_compile_definitions(${target}
        PRIVATE WAVE_TOOLCHAIN_FIXTURE_MODE=${mode})
    wave_enable_warnings(${target})
endfunction()

find_package(Qt6 6.10.2 EXACT REQUIRED COMPONENTS Test)
add_executable(wave-ui-capabilities-test ui_capabilities_test.cpp)
target_link_libraries(wave-ui-capabilities-test PRIVATE wavewidgets waveproject Qt6::Test Ela::WidgetTools)
wave_enable_warnings(wave-ui-capabilities-test)
foreach(_wave_capability_scale IN ITEMS 1 2)
    add_test(NAME wave-ui-capabilities-${_wave_capability_scale}
        COMMAND "${CMAKE_COMMAND}"
            "-DTEST_EXE=$<TARGET_FILE:wave-ui-capabilities-test>"
            "-DLOG_FILE=${CMAKE_BINARY_DIR}/ui-capabilities-${_wave_capability_scale}.txt"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_ela_ui_test.cmake")
    set_tests_properties(wave-ui-capabilities-${_wave_capability_scale} PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_SCALE_FACTOR=${_wave_capability_scale};WAVEWORKBENCH_REDUCED_MOTION=0;QT_QPA_FONTDIR=${CMAKE_BINARY_DIR}/test-fonts;WAVEWORKBENCH_SETTINGS_DIR=${CMAKE_BINARY_DIR}/test-settings/capabilities-${_wave_capability_scale};WAVEWORKBENCH_RECOVERY_DIR=${CMAKE_BINARY_DIR}/test-recovery/capabilities-${_wave_capability_scale}"
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${CMAKE_BINARY_DIR};PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
        RUN_SERIAL TRUE TIMEOUT 45)
    if(WIN32)
        add_test(NAME wave-ui-capabilities-native-${_wave_capability_scale}
            COMMAND "${CMAKE_COMMAND}"
                "-DTEST_EXE=$<TARGET_FILE:wave-ui-capabilities-test>"
                "-DTEST_FUNCTIONS=interruptibleCombo;comboPopupRowsFit;focusAnimationsAreOwned;focusedViewDestruction;browsingScrollAndExactTimeline;replacedOverlayScrollBarOrigin;tabsKeepQtPageOwnership;menuKeepsActionsAndLiveEditors;simulationDrawersPreserveData"
                "-DLOG_FILE=${CMAKE_BINARY_DIR}/ui-capabilities-native-${_wave_capability_scale}.txt"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/run_ela_ui_test.cmake")
        set_tests_properties(wave-ui-capabilities-native-${_wave_capability_scale} PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=windows;QT_SCALE_FACTOR=${_wave_capability_scale};WAVEWORKBENCH_REDUCED_MOTION=0;QT_QPA_FONTDIR=${CMAKE_BINARY_DIR}/test-fonts;WAVEWORKBENCH_SETTINGS_DIR=${CMAKE_BINARY_DIR}/test-settings/capabilities-native-${_wave_capability_scale};WAVEWORKBENCH_RECOVERY_DIR=${CMAKE_BINARY_DIR}/test-recovery/capabilities-native-${_wave_capability_scale}"
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${CMAKE_BINARY_DIR};PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
            RUN_SERIAL TRUE TIMEOUT 45)
    endif()
endforeach()
add_executable(wave-ui-followup-test ui_followup_test.cpp)
target_link_libraries(wave-ui-followup-test PRIVATE wavewidgets waveproject Qt6::Test Ela::WidgetTools)
wave_enable_warnings(wave-ui-followup-test)
foreach(_wave_followup_scale IN ITEMS 1 1.25 1.5 2)
    add_test(NAME wave-ui-followup-${_wave_followup_scale}
        COMMAND "${CMAKE_COMMAND}"
            "-DTEST_EXE=$<TARGET_FILE:wave-ui-followup-test>"
            "-DLOG_FILE=${CMAKE_BINARY_DIR}/ui-followup-${_wave_followup_scale}.txt"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_ela_ui_test.cmake")
    set_tests_properties(wave-ui-followup-${_wave_followup_scale} PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_SCALE_FACTOR=${_wave_followup_scale};WAVEWORKBENCH_REDUCED_MOTION=1;WAVE_FOLLOWUP_PREVIEW_DIR=${CMAKE_BINARY_DIR}/ui-followup-previews/${_wave_followup_scale};WAVEWORKBENCH_RECOVERY_DIR=${CMAKE_BINARY_DIR}/test-recovery/followup-${_wave_followup_scale};WAVEWORKBENCH_SETTINGS_DIR=${CMAKE_BINARY_DIR}/test-settings/followup-${_wave_followup_scale}"
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${CMAKE_BINARY_DIR};PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
        TIMEOUT 45)
endforeach()
if(WIN32)
    foreach(_wave_native_scale IN ITEMS 1 1.25 1.5 2)
        add_test(NAME wave-ui-native-${_wave_native_scale}
            COMMAND "${CMAKE_COMMAND}"
                "-DTEST_EXE=$<TARGET_FILE:wave-ui-followup-test>"
                "-DTEST_FUNCTIONS=firstShowGeometry;chromeAndExactUnits;resizeFitsAndPreservesContext;styleApplyViaPopupAndButton;stylePreviewIsLiveAndIsolated;englishBuiltInText;titleClosePreservesCancelledDraft"
                "-DLOG_FILE=${CMAKE_BINARY_DIR}/ui-native-${_wave_native_scale}.txt"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/run_ela_ui_test.cmake")
        set_tests_properties(wave-ui-native-${_wave_native_scale} PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=windows;QT_SCALE_FACTOR=${_wave_native_scale};WAVEWORKBENCH_REDUCED_MOTION=0;WAVE_FOLLOWUP_PREVIEW_DIR=${CMAKE_BINARY_DIR}/ui-native-previews/${_wave_native_scale};WAVEWORKBENCH_RECOVERY_DIR=${CMAKE_BINARY_DIR}/test-recovery/native-${_wave_native_scale};WAVEWORKBENCH_SETTINGS_DIR=${CMAKE_BINARY_DIR}/test-settings/native-${_wave_native_scale}"
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${CMAKE_BINARY_DIR};PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
            TIMEOUT 45
            RUN_SERIAL TRUE)
    endforeach()
endif()
add_executable(wave-canvas-edit-regression-test canvas_edit_regression_test.cpp)
target_link_libraries(wave-canvas-edit-regression-test PRIVATE wavewidgets waveproject wavegenerate Qt6::Test)
wave_enable_warnings(wave-canvas-edit-regression-test)
add_test(NAME wave-canvas-edit-regression-test
    COMMAND "${CMAKE_COMMAND}"
        "-DTEST_EXE=$<TARGET_FILE:wave-canvas-edit-regression-test>"
        "-DLOG_FILE=${CMAKE_BINARY_DIR}/canvas-edit-regression.txt"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_ela_ui_test.cmake")
set_tests_properties(wave-canvas-edit-regression-test PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_SCALE_FACTOR=1;WAVEWORKBENCH_REDUCED_MOTION=1;WAVE_CANVAS_PREVIEW_DIR=${CMAKE_BINARY_DIR}/canvas-previews"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${CMAKE_BINARY_DIR};PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    TIMEOUT 30)
add_executable(wave-canvas-overlay-index-test canvas_overlay_index_test.cpp)
target_link_libraries(wave-canvas-overlay-index-test PRIVATE wavewidgets waveproject Qt6::Test)
wave_enable_warnings(wave-canvas-overlay-index-test)
add_test(NAME wave-canvas-overlay-index-test
    COMMAND "${CMAKE_COMMAND}"
        "-DTEST_EXE=$<TARGET_FILE:wave-canvas-overlay-index-test>"
        "-DLOG_FILE=${CMAKE_BINARY_DIR}/canvas-overlay-index.txt"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_ela_ui_test.cmake")
set_tests_properties(wave-canvas-overlay-index-test PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_SCALE_FACTOR=1;WAVEWORKBENCH_REDUCED_MOTION=1;QT_QPA_FONTDIR=${CMAKE_BINARY_DIR}/test-fonts;WAVE_OVERLAY_EVIDENCE_DIR=${CMAKE_BINARY_DIR}/overlay-index-evidence"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${CMAKE_BINARY_DIR};PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    TIMEOUT 30)
add_executable(wave-ela-ui-test ela_ui_test.cpp)
target_link_libraries(wave-ela-ui-test PRIVATE wavewidgets Qt6::Test Ela::WidgetTools)
wave_enable_warnings(wave-ela-ui-test)
foreach(_wave_ela_scale IN ITEMS 1 1.25 1.5 2)
    add_test(NAME wave-ela-ui-${_wave_ela_scale}
        COMMAND "${CMAKE_COMMAND}"
            "-DTEST_EXE=$<TARGET_FILE:wave-ela-ui-test>"
            "-DLOG_FILE=${CMAKE_BINARY_DIR}/ela-ui-${_wave_ela_scale}.txt"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_ela_ui_test.cmake")
    set_tests_properties(wave-ela-ui-${_wave_ela_scale} PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_SCALE_FACTOR=${_wave_ela_scale};WAVEWORKBENCH_REDUCED_MOTION=1;WAVE_ELA_PREVIEW_DIR=${CMAKE_BINARY_DIR}/ela-previews/${_wave_ela_scale};WAVEWORKBENCH_RECOVERY_DIR=${CMAKE_BINARY_DIR}/test-recovery/ela-${_wave_ela_scale};WAVEWORKBENCH_SETTINGS_DIR=${CMAKE_BINARY_DIR}/test-settings/ela-${_wave_ela_scale}"
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${CMAKE_BINARY_DIR};PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
        TIMEOUT 30)
endforeach()

if(SuiteApp_FOUND)
    add_executable(wave-suiteapp-runtime-test suiteapp_runtime_test.cpp)
    target_link_libraries(wave-suiteapp-runtime-test
        PRIVATE wavewidgets waveproject SuiteApp::suiteapp Qt6::Test Qt6::Concurrent)
    target_compile_definitions(wave-suiteapp-runtime-test PRIVATE
        WAVE_TEST_APPLICATION_VERSION="${PROJECT_VERSION}"
        WAVE_SUITE_RUNTIME_PATH="$<TARGET_FILE:SuiteApp::suite-runtime>")
    wave_enable_warnings(wave-suiteapp-runtime-test)
    add_test(NAME wave-suiteapp-runtime-test
        COMMAND "${CMAKE_COMMAND}"
            "-DTEST_EXE=$<TARGET_FILE:wave-suiteapp-runtime-test>"
            "-DLOG_FILE=${CMAKE_BINARY_DIR}/suiteapp-runtime.txt"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_ela_ui_test.cmake")
    set_tests_properties(wave-suiteapp-runtime-test PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${CMAKE_BINARY_DIR};PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
        TIMEOUT 45)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Test)
    add_executable(wave-suiteapp-integration-test
        suiteapp_integration_test.cpp)
    target_link_libraries(wave-suiteapp-integration-test
        PRIVATE wavewidgets waveproject SuiteApp::suiteapp Qt6::Test)
    wave_enable_warnings(wave-suiteapp-integration-test)
    add_test(NAME wave-suiteapp-integration-test
        COMMAND wave-suiteapp-integration-test)
    set_tests_properties(wave-suiteapp-integration-test PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${CMAKE_BINARY_DIR};PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")
endif()

wave_add_toolchain_fixture(wave-toolchain-fixture-ready 1)
wave_add_toolchain_fixture(wave-toolchain-fixture-old 2)
wave_add_toolchain_fixture(wave-toolchain-fixture-failure 3)
wave_add_toolchain_fixture(wave-toolchain-fixture-hang 4)
wave_add_toolchain_fixture(wave-verilator-fixture 5)
wave_add_toolchain_fixture(wave-simulator-fixture 6)

add_executable(wave-tests
    test_main.cpp
)
target_link_libraries(wave-tests
    PRIVATE wavetime wavecore waveedit waveproject wavevalidate wavegenerate waveimport wavefst wavecompare waveintegration wavesim waveautomation
            Qt6::Core Qt6::Gui Qt6::Svg
)
add_dependencies(wave-tests
    wave-toolchain-fixture-ready
    wave-toolchain-fixture-old
    wave-toolchain-fixture-failure
    wave-toolchain-fixture-hang
    wave-verilator-fixture
    wave-simulator-fixture)
wave_enable_warnings(wave-tests)
target_compile_definitions(wave-tests
    PRIVATE WAVE_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)
if(WAVEWORKBENCH_ENABLE_WELLEN)
    add_dependencies(wave-tests wave-wellen-reader)
    target_compile_definitions(wave-tests
        PRIVATE WAVE_WELLEN_READER="${WAVE_WELLEN_READER_OUTPUT}")
endif()

add_executable(wave-fixed-fixture-ui-smoke
    fixed_fixture_ui_smoke.cpp
)
target_link_libraries(wave-fixed-fixture-ui-smoke
    PRIVATE wavewidgets wavesim waveintegration wavetrace wavecore
            Qt6::Core Qt6::Gui Qt6::Widgets)
target_compile_definitions(wave-fixed-fixture-ui-smoke
    PRIVATE WAVE_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
add_dependencies(wave-fixed-fixture-ui-smoke
    wave-toolchain-fixture-ready
    wave-verilator-fixture
    wave-simulator-fixture)
wave_enable_warnings(wave-fixed-fixture-ui-smoke)

add_executable(wave-expected-actual-ui-smoke
    expected_actual_ui_smoke.cpp
)
target_link_libraries(wave-expected-actual-ui-smoke
    PRIVATE wavewidgets wavesim waveintegration waveproject wavetrace wavecore
            Qt6::Core Qt6::Gui Qt6::Widgets)
target_compile_definitions(wave-expected-actual-ui-smoke
    PRIVATE WAVE_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
add_dependencies(wave-expected-actual-ui-smoke
    wave-toolchain-fixture-ready
    wave-verilator-fixture
    wave-simulator-fixture)
wave_enable_warnings(wave-expected-actual-ui-smoke)

add_executable(wave-structured-input-ui-smoke
    structured_input_ui_smoke.cpp
)
target_link_libraries(wave-structured-input-ui-smoke
    PRIVATE wavewidgets waveintegration waveedit wavecore
            Qt6::Core Qt6::Gui Qt6::Widgets)
target_compile_definitions(wave-structured-input-ui-smoke
    PRIVATE WAVE_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
wave_enable_warnings(wave-structured-input-ui-smoke)

add_executable(wave-stub-selection-ui-smoke
    stub_selection_ui_smoke.cpp
)
target_link_libraries(wave-stub-selection-ui-smoke
    PRIVATE wavewidgets wavesim waveintegration waveproject wavecore
            Qt6::Core Qt6::Gui Qt6::Widgets)
target_compile_definitions(wave-stub-selection-ui-smoke
    PRIVATE WAVE_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
wave_enable_warnings(wave-stub-selection-ui-smoke)

add_executable(wave-multi-scenario-batch-ui-smoke
    multi_scenario_batch_ui_smoke.cpp
)
target_link_libraries(wave-multi-scenario-batch-ui-smoke
    PRIVATE wavewidgets wavesim waveintegration waveproject wavecore
            Qt6::Core Qt6::Gui Qt6::Widgets)
target_compile_definitions(wave-multi-scenario-batch-ui-smoke
    PRIVATE WAVE_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
add_dependencies(wave-multi-scenario-batch-ui-smoke
    wave-toolchain-fixture-ready
    wave-verilator-fixture
    wave-simulator-fixture)
wave_enable_warnings(wave-multi-scenario-batch-ui-smoke)

add_executable(wave-source-navigation-ui-smoke
    source_navigation_ui_smoke.cpp
)
target_link_libraries(wave-source-navigation-ui-smoke
    PRIVATE wavewidgets wavesim waveintegration waveproject wavecore
            Qt6::Core Qt6::Gui Qt6::Widgets)
target_compile_definitions(wave-source-navigation-ui-smoke
    PRIVATE WAVE_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
add_dependencies(wave-source-navigation-ui-smoke
    wave-toolchain-fixture-ready
    wave-verilator-fixture
    wave-simulator-fixture)
wave_enable_warnings(wave-source-navigation-ui-smoke)

if(WAVEWORKBENCH_ENABLE_WELLEN)
    add_executable(wave-fst-on-demand-ui-smoke
        fst_on_demand_ui_smoke.cpp
    )
    target_link_libraries(wave-fst-on-demand-ui-smoke
        PRIVATE wavewidgets wavefst wavecore
                Qt6::Core Qt6::Gui Qt6::Widgets)
    target_compile_definitions(wave-fst-on-demand-ui-smoke
        PRIVATE
            WAVE_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
            WAVE_WELLEN_READER="${WAVE_WELLEN_READER_OUTPUT}")
    add_dependencies(wave-fst-on-demand-ui-smoke wave-wellen-reader)
    wave_enable_warnings(wave-fst-on-demand-ui-smoke)
    add_test(
        NAME wave-fst-on-demand-ui-smoke
        COMMAND wave-fst-on-demand-ui-smoke
                "${CMAKE_BINARY_DIR}/fst-on-demand-ui-smoke.png"
    )
endif()

add_executable(wave-timeline-viewport-test timeline_viewport_test.cpp)
target_link_libraries(wave-timeline-viewport-test PRIVATE wavetime)
wave_enable_warnings(wave-timeline-viewport-test)

add_executable(wavewidgets-abi-test wavewidgets_abi_test.cpp)
target_link_libraries(wavewidgets-abi-test
    PRIVATE wavewidgets waveproject Qt6::Core Qt6::Gui Qt6::Widgets)
target_compile_definitions(wavewidgets-abi-test
    PRIVATE WAVE_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
if(WAVEWORKBENCH_ENABLE_WELLEN)
    target_compile_definitions(wavewidgets-abi-test
        PRIVATE WAVE_WELLEN_READER="${WAVE_WELLEN_READER_OUTPUT}")
endif()
wave_enable_warnings(wavewidgets-abi-test)

add_executable(waveform-view-ui-smoke waveform_view_ui_smoke.cpp)
target_link_libraries(waveform-view-ui-smoke
    PRIVATE wavewidgets Qt6::Core Qt6::Gui Qt6::Widgets)
wave_enable_warnings(waveform-view-ui-smoke)

add_executable(wave-project-atomic-save-test project_atomic_save_test.cpp)
target_link_libraries(wave-project-atomic-save-test PRIVATE waveproject Qt6::Test)
wave_enable_warnings(wave-project-atomic-save-test)
add_test(NAME wave-project-atomic-save-test
    COMMAND "${CMAKE_COMMAND}"
        "-DTEST_EXE=$<TARGET_FILE:wave-project-atomic-save-test>"
        "-DLOG_FILE=${CMAKE_BINARY_DIR}/project-atomic-save.txt"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_ela_ui_test.cmake")
set_tests_properties(wave-project-atomic-save-test PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${CMAKE_BINARY_DIR};PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    TIMEOUT 30)

add_executable(wave-external-reload-ui-smoke external_reload_ui_smoke.cpp)
target_link_libraries(wave-external-reload-ui-smoke
    PRIVATE wavewidgets waveproject
            Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Test)
wave_enable_warnings(wave-external-reload-ui-smoke)

add_executable(wave-large-canvas-benchmark large_canvas_benchmark.cpp)
target_link_libraries(wave-large-canvas-benchmark
    PRIVATE wavewidgets waveedit wavecore
            Qt6::Core Qt6::Gui Qt6::Widgets)
if(WIN32)
    target_link_libraries(wave-large-canvas-benchmark PRIVATE psapi)
endif()
wave_enable_warnings(wave-large-canvas-benchmark)

add_executable(wave-relation-edit-ui-smoke relation_edit_ui_smoke.cpp)
target_link_libraries(wave-relation-edit-ui-smoke
    PRIVATE wavewidgets waveedit wavecore
            Qt6::Core Qt6::Gui Qt6::Widgets)
wave_enable_warnings(wave-relation-edit-ui-smoke)

add_test(NAME wave-core-tests COMMAND wave-tests)
add_test(
    NAME wave-expected-actual-ui-smoke
    COMMAND wave-expected-actual-ui-smoke
            "${CMAKE_BINARY_DIR}/expected-actual-ui-smoke.png"
)
add_test(
    NAME wave-structured-input-ui-smoke
    COMMAND wave-structured-input-ui-smoke
            "${CMAKE_BINARY_DIR}/structured-input-ui-smoke.png"
)
add_test(
    NAME wave-stub-selection-ui-smoke
    COMMAND wave-stub-selection-ui-smoke
            "${CMAKE_BINARY_DIR}/stub-selection-ui-smoke.png"
)
add_test(
    NAME wave-multi-scenario-batch-ui-smoke
    COMMAND wave-multi-scenario-batch-ui-smoke
            "${CMAKE_BINARY_DIR}/multi-scenario-batch-ui-smoke.png"
)
add_test(
    NAME wave-source-navigation-ui-smoke
    COMMAND wave-source-navigation-ui-smoke
            "${CMAKE_BINARY_DIR}/source-navigation-ui-smoke.png"
)
add_test(NAME wave-timeline-viewport-test COMMAND wave-timeline-viewport-test)
add_test(NAME wavewidgets-abi-test COMMAND wavewidgets-abi-test)
add_test(NAME wave-external-reload-ui-smoke
    COMMAND wave-external-reload-ui-smoke)
add_test(NAME wave-large-canvas-benchmark
    COMMAND wave-large-canvas-benchmark)
add_test(NAME wave-relation-edit-ui-smoke
    COMMAND wave-relation-edit-ui-smoke)
set(_waveform_view_scale_tests)
set(_waveform_view_scales 1.00 1.25 1.50 2.00)
foreach(_waveform_scale IN LISTS _waveform_view_scales)
    string(REPLACE "." "" _waveform_scale_tag "${_waveform_scale}")
    set(_waveform_scale_test
        "waveform-view-ui-scale-${_waveform_scale_tag}")
    add_test(
        NAME ${_waveform_scale_test}
        COMMAND waveform-view-ui-smoke
                "${CMAKE_BINARY_DIR}/${_waveform_scale_test}-compact.png"
                "${_waveform_scale}"
    )
    list(APPEND _waveform_view_scale_tests ${_waveform_scale_test})
endforeach()
add_test(NAME wave-app-smoke COMMAND Tickx --smoke-test)
add_test(NAME tickx-branding-contract
    COMMAND "${CMAKE_COMMAND}" "-DAPP=$<TARGET_FILE:Tickx>" "-DEXPECTED_VERSION=${PROJECT_VERSION}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tickx_branding_contract.cmake")
set_tests_properties(tickx-branding-contract PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${CMAKE_BINARY_DIR};PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
    TIMEOUT 15)
add_test(
    NAME wave-example-smoke
    COMMAND Tickx --smoke-test "${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
)
add_test(
    NAME wave-simulation-result-window-smoke
    COMMAND Tickx
            --load-first-trace
            "--screenshot=${CMAKE_BINARY_DIR}/simulation-result-window-smoke.png"
            "${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
)
add_test(
    NAME wave-autosave-smoke
    COMMAND Tickx
            "--autosave-smoke=${CMAKE_BINARY_DIR}/autosave-recovery.wave.json"
            "${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
)
add_test(
    NAME wave-file-conflict-smoke
    COMMAND Tickx
            # Other tests' build-root output must not reset the directory debounce.
            "--file-conflict-smoke=${CMAKE_BINARY_DIR}/test-projects/file-conflict/project.wave.json"
)
add_test(
    NAME wave-lane-dialog-smoke
    COMMAND Tickx
            "--lane-dialog-screenshot=${CMAKE_BINARY_DIR}/lane-dialog-smoke.png"
            "${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
)
add_test(
    NAME wave-lane-removal-smoke
    COMMAND Tickx --lane-removal-smoke
)
add_test(
    NAME wave-lane-reorder-smoke
    COMMAND Tickx --lane-reorder-smoke
)
add_test(
    NAME wave-lane-autoscroll-smoke
    COMMAND Tickx
            "--lane-autoscroll-smoke=${CMAKE_BINARY_DIR}/lane-autoscroll-smoke.png"
            "${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
)
configure_file(
    "${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
    "${CMAKE_BINARY_DIR}/hidden-lane-smoke-project.wave.json"
    COPYONLY)
add_test(
    NAME wave-hidden-lane-smoke
    COMMAND Tickx
            "--hidden-lane-smoke=${CMAKE_BINARY_DIR}/hidden-lane-smoke-quick-hide.png"
            "${CMAKE_BINARY_DIR}/hidden-lane-smoke-project.wave.json"
)
add_test(
    NAME wave-group-header-smoke
    COMMAND Tickx
            "--group-header-smoke=${CMAKE_BINARY_DIR}/group-collapse-smoke.png"
            "${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
)
add_test(
    NAME wave-group-creation-smoke
    COMMAND Tickx
            "--group-creation-smoke=${CMAKE_BINARY_DIR}/group-creation-smoke.png"
            "${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
)
add_test(
    NAME wave-group-batch-smoke
    COMMAND Tickx
            "--group-batch-smoke=${CMAKE_BINARY_DIR}/group-batch-smoke.png"
            "${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
)
add_test(
    NAME wave-group-membership-smoke
    COMMAND Tickx
            "--group-membership-smoke=${CMAKE_BINARY_DIR}/group-membership-smoke.png"
            "${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
)
add_test(
    NAME wave-signal-header-smoke
    COMMAND Tickx
            --signal-header-smoke
            "${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
)
add_test(
    NAME wave-canvas-add-lane-smoke
    COMMAND Tickx
            "--canvas-add-lane-smoke=${CMAKE_BINARY_DIR}/canvas-signal-management-smoke.png"
)
add_test(
    NAME wave-cursor-mode-smoke
    COMMAND Tickx
            "--cursor-mode-smoke=${CMAKE_BINARY_DIR}/cursor-mode-smoke.png"
)
add_test(
    NAME wave-wave-edit-smoke
    COMMAND Tickx
            "--wave-edit-smoke=${CMAKE_BINARY_DIR}/wave-edit-smoke.png"
)
add_test(
    NAME wave-wave-edit-autoscroll-smoke
    COMMAND Tickx
            "--wave-edit-autoscroll-smoke=${CMAKE_BINARY_DIR}/wave-edit-autoscroll-smoke.png"
            "${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
)
add_test(
    NAME wave-new-project-smoke
    COMMAND Tickx --new-project-smoke
)
add_test(
    NAME wave-user-journey-smoke
    COMMAND Tickx
            "--user-journey-smoke=${CMAKE_BINARY_DIR}/user-journey.wave.json"
)
add_test(
    NAME wave-edit-menu-smoke
    COMMAND Tickx
            "--edit-menu-screenshot=${CMAKE_BINARY_DIR}/edit-menu-smoke.png"
)
add_test(
    NAME wave-waveform-only-smoke
    COMMAND Tickx
            "--waveform-only-smoke=${CMAKE_BINARY_DIR}/waveform-only-smoke.png"
)
add_test(
    NAME wave-uri-smoke
    COMMAND Tickx --smoke-test
            "wave://scenario/scenario-handshake?project=${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json&lane=lane-data&marker=marker-transfer"
)
add_test(
    NAME wave-uri-invalid-id-smoke
    COMMAND Tickx --smoke-test
            "wave://scenario/missing-scenario?project=${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
)
set_tests_properties(wave-uri-invalid-id-smoke PROPERTIES WILL_FAIL TRUE)
add_test(
    NAME wave-scenario-switch-smoke
    COMMAND Tickx
            "--scenario-switch-smoke=${CMAKE_BINARY_DIR}/scenario-switch-smoke.png"
)
add_test(
    NAME wave-generate-smoke
    COMMAND wave-generate
            "${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "${CMAKE_BINARY_DIR}/cli-generated"
)
add_test(
    NAME wave-compare-smoke
    COMMAND wave-compare
            "${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "${CMAKE_BINARY_DIR}/cli-compare"
)
add_test(
    NAME wave-bridge-describe-smoke
    COMMAND wave-bridge describe
            "${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "${CMAKE_BINARY_DIR}/bridge-workspace.json"
)
add_test(
    NAME wave-bridge-signals-smoke
    COMMAND wave-bridge import-signals
            "${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "${CMAKE_SOURCE_DIR}/examples/handshake/integration/zeroslack_signals.json"
            "${CMAKE_BINARY_DIR}/bridge-signals.wave.json"
)
add_test(
    NAME wave-bridge-module-manifest-smoke
    COMMAND wave-bridge import-module
            "${CMAKE_SOURCE_DIR}/examples/handshake/integration/zeroslack_module_manifest_v1.json"
            "${CMAKE_BINARY_DIR}/bridge-module.wave.json"
)
add_test(
    NAME wave-bridge-module-manifest-load-smoke
    COMMAND wave-cli inspect
            "${CMAKE_BINARY_DIR}/bridge-module.wave.json"
)
set_tests_properties(wave-bridge-module-manifest-load-smoke PROPERTIES
    DEPENDS wave-bridge-module-manifest-smoke TIMEOUT 60)
add_test(
    NAME wave-bridge-stimulus-export-smoke
    COMMAND wave-bridge export-stimulus
            "${CMAKE_BINARY_DIR}/bridge-module.wave.json"
            "${CMAKE_BINARY_DIR}/bridge-stimulus.json"
)
set_tests_properties(wave-bridge-stimulus-export-smoke PROPERTIES
    DEPENDS wave-bridge-module-manifest-smoke TIMEOUT 60)
add_test(
    NAME wave-bridge-stimulus-import-smoke
    COMMAND wave-bridge import-stimulus
            "${CMAKE_SOURCE_DIR}/examples/handshake/integration/zeroslack_module_manifest_v1.json"
            "${CMAKE_BINARY_DIR}/bridge-stimulus.json"
            "${CMAKE_BINARY_DIR}/bridge-stimulus-restored.wave.json"
)
set_tests_properties(wave-bridge-stimulus-import-smoke PROPERTIES
    DEPENDS wave-bridge-stimulus-export-smoke TIMEOUT 60)
add_test(
    NAME wave-bridge-stimulus-load-smoke
    COMMAND wave-cli inspect
            "${CMAKE_BINARY_DIR}/bridge-stimulus-restored.wave.json"
)
set_tests_properties(wave-bridge-stimulus-load-smoke PROPERTIES
    DEPENDS wave-bridge-stimulus-import-smoke TIMEOUT 60)
add_test(
    NAME wave-bridge-stimulus-validate-smoke
    COMMAND wave-cli validate
            "${CMAKE_BINARY_DIR}/bridge-stimulus-restored.wave.json"
)
set_tests_properties(wave-bridge-stimulus-validate-smoke PROPERTIES
    DEPENDS wave-bridge-stimulus-import-smoke TIMEOUT 60)
add_test(
    NAME wave-bridge-frame-smoke
    COMMAND wave-bridge link-frame
            "${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "${CMAKE_SOURCE_DIR}/examples/handshake/integration/frame_sample_reference.json"
            "${CMAKE_BINARY_DIR}/bridge-frame.wave.json"
)
add_test(
    NAME wave-bridge-pinloom-smoke
    COMMAND wave-bridge pinloom-entry
            "${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "${CMAKE_SOURCE_DIR}/examples/handshake/exports"
            "${CMAKE_BINARY_DIR}/bridge-pinloom.json"
)
add_test(
    NAME wave-cli-inspect-smoke
    COMMAND wave-cli inspect
            "${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            --scenario=scenario-handshake
)
add_test(
    NAME wave-cli-validate-smoke
    COMMAND wave-cli validate
            "${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            --scenario=scenario-handshake
)
add_test(
    NAME wave-cli-summary-smoke
    COMMAND wave-cli inspect
            "${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            --scenario=scenario-handshake
            --summary
)
add_test(
    NAME wave-cli-sample-smoke
    COMMAND wave-cli sample
            "${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "--at=80 ns"
            --scenario=scenario-handshake
            --lane=lane-request
            --lane=lane-data
)
add_test(
    NAME wave-cli-window-smoke
    COMMAND wave-cli window
            "${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "--start=70 ns"
            "--end=130 ns"
            --scenario=scenario-handshake
            --lane=lane-request
            --lane=lane-data
)
add_test(
    NAME wave-cli-apply-dry-run-smoke
    COMMAND wave-cli apply
            "${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "${CMAKE_SOURCE_DIR}/tests/fixtures/cli-operations.json"
            "--scenario=Request / acknowledge"
            --dry-run
)
add_test(
    NAME wave-cli-apply-smoke
    COMMAND wave-cli apply
            "${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "${CMAKE_SOURCE_DIR}/tests/fixtures/cli-operations.json"
            "--output=${CMAKE_BINARY_DIR}/cli-applied.wave.json"
)
add_test(
    NAME wave-cli-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DPROJECT=${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "-DOPERATIONS=${CMAKE_SOURCE_DIR}/tests/fixtures/cli-operations.json"
            "-DOUTPUT=${CMAKE_BINARY_DIR}/cli-contract-applied.wave.json"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_contract_smoke.cmake"
)
add_test(
    NAME wave-cli-advanced-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DPROJECT=${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "-DOPERATIONS=${CMAKE_SOURCE_DIR}/tests/fixtures/cli-operations-advanced.json"
            "-DOUTPUT=${CMAKE_BINARY_DIR}/cli-advanced-applied.wave.json"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_advanced_contract_smoke.cmake"
)
add_test(
    NAME wave-cli-window-range-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DPROJECT=${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "-DOPERATIONS=${CMAKE_SOURCE_DIR}/tests/fixtures/cli-operations-range.json"
            "-DCONFLICT_OPERATIONS=${CMAKE_SOURCE_DIR}/tests/fixtures/cli-operations-transfer-conflict.json"
            "-DASSERT_OPERATIONS=${CMAKE_SOURCE_DIR}/tests/fixtures/cli-operations-assert-fail.json"
            "-DOUTPUT=${CMAKE_BINARY_DIR}/cli-window-range-applied.wave.json"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_window_range_contract_smoke.cmake"
)
add_test(
    NAME wave-cli-new-dry-run-smoke
    COMMAND wave-cli new
            "${CMAKE_BINARY_DIR}/cli-new-direct-smoke"
            "--name=Direct smoke"
            "--duration=60 ns"
            --dry-run
)
add_test(
    NAME wave-cli-new-sequence-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DOPERATIONS=${CMAKE_SOURCE_DIR}/tests/fixtures/cli-new-operations.json"
            "-DINVALID_OPERATIONS=${CMAKE_SOURCE_DIR}/tests/fixtures/cli-new-operations-invalid.json"
            "-DOUTPUT=${CMAKE_BINARY_DIR}/cli-new-sequence-contract"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_new_sequence_contract_smoke.cmake"
)
add_test(
    NAME wave-cli-signals-smoke
    COMMAND wave-cli signals
            "${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "--match=req"
            --kind=bit
            --exact
)
add_test(
    NAME wave-cli-selector-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DPROJECT=${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "-DNAME_OPERATIONS=${CMAKE_SOURCE_DIR}/tests/fixtures/cli-new-operations-names.json"
            "-DNAME_EDIT_OPERATIONS=${CMAKE_SOURCE_DIR}/tests/fixtures/cli-name-edit-operations.json"
            "-DOUTPUT=${CMAKE_BINARY_DIR}/cli-selector-contract"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_selector_contract_smoke.cmake"
)
add_test(
    NAME wave-cli-symbolic-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DNEW_OPERATIONS=${CMAKE_SOURCE_DIR}/tests/fixtures/cli-symbolic-new-operations.json"
            "-DUPDATE_OPERATIONS=${CMAKE_SOURCE_DIR}/tests/fixtures/cli-symbolic-update-operations.json"
            "-DINVALID_OPERATIONS=${CMAKE_SOURCE_DIR}/tests/fixtures/cli-symbolic-invalid-map.json"
            "-DOUTPUT=${CMAKE_BINARY_DIR}/cli-symbolic-contract"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_symbolic_contract_smoke.cmake"
)
add_test(
    NAME wave-cli-intent-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DNEW_OPERATIONS=${CMAKE_SOURCE_DIR}/tests/fixtures/cli-symbolic-new-operations.json"
            "-DINTENT_OPERATIONS=${CMAKE_SOURCE_DIR}/tests/fixtures/cli-intent-operations.json"
            "-DCLEANUP_OPERATIONS=${CMAKE_SOURCE_DIR}/tests/fixtures/cli-intent-cleanup-operations.json"
            "-DINVALID_OPERATIONS=${CMAKE_SOURCE_DIR}/tests/fixtures/cli-intent-invalid-edge.json"
            "-DOUTPUT=${CMAKE_BINARY_DIR}/cli-intent-contract"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_intent_contract_smoke.cmake"
)
add_test(
    NAME wave-cli-capabilities-group-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DGROUP_OPERATIONS=${CMAKE_SOURCE_DIR}/tests/fixtures/cli-group-lifecycle-operations.json"
            "-DCLEANUP_OPERATIONS=${CMAKE_SOURCE_DIR}/tests/fixtures/cli-group-cleanup-operations.json"
            "-DINVALID_OPERATIONS=${CMAKE_SOURCE_DIR}/tests/fixtures/cli-group-invalid-operations.json"
            "-DOUTPUT=${CMAKE_BINARY_DIR}/cli-capabilities-group-contract"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_capabilities_group_contract_smoke.cmake"
)
add_test(
    NAME wave-cli-duplicate-signal-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DPROJECT=${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "-DOPERATIONS=${CMAKE_SOURCE_DIR}/tests/fixtures/cli-duplicate-signal-operations.json"
            "-DINVALID_OPERATIONS=${CMAKE_SOURCE_DIR}/tests/fixtures/cli-duplicate-signal-invalid.json"
            "-DOUTPUT=${CMAKE_BINARY_DIR}/cli-duplicate-signal-contract"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_duplicate_signal_contract_smoke.cmake"
)
add_test(
    NAME wave-cli-duration-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DPROJECT=${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "-DTRUNCATE_OPERATIONS=${CMAKE_SOURCE_DIR}/tests/fixtures/cli-duration-truncate-operations.json"
            "-DGUARDED_OPERATIONS=${CMAKE_SOURCE_DIR}/tests/fixtures/cli-duration-guarded-operations.json"
            "-DSAFE_OPERATIONS=${CMAKE_SOURCE_DIR}/tests/fixtures/cli-duration-safe-shrink-operations.json"
            "-DOUTPUT=${CMAKE_BINARY_DIR}/cli-duration-contract"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_duration_contract_smoke.cmake"
)
add_test(
    NAME wave-cli-edges-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DPROJECT=${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_edges_contract_smoke.cmake"
)
add_test(
    NAME wave-cli-relations-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DPROJECT=${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_relations_contract_smoke.cmake"
)
add_test(
    NAME wave-cli-relation-repair-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DPROJECT=${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "-DOUTPUT=${CMAKE_BINARY_DIR}/cli-relation-repair-contract"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_relation_repair_contract_smoke.cmake"
)
add_test(
    NAME wave-cli-relation-identity-repair-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DPROJECT=${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "-DOUTPUT=${CMAKE_BINARY_DIR}/cli-relation-identity-repair-contract"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_relation_identity_repair_contract_smoke.cmake"
)
add_test(
    NAME wave-cli-identity-validation-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DPROJECT=${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "-DOUTPUT=${CMAKE_BINARY_DIR}/cli-identity-validation-contract"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_identity_validation_contract_smoke.cmake"
)
add_test(
    NAME wave-cli-post-validation-guard-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DPROJECT=${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "-DOUTPUT=${CMAKE_BINARY_DIR}/cli-post-validation-guard-contract"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_post_validation_guard_contract_smoke.cmake"
)
add_test(
    NAME wave-cli-markers-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DPROJECT=${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_markers_contract_smoke.cmake"
)
add_test(
    NAME wave-cli-marker-repair-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DPROJECT=${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "-DOUTPUT=${CMAKE_BINARY_DIR}/cli-marker-repair-contract"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_marker_repair_contract_smoke.cmake"
)
add_test(
    NAME wave-cli-marker-validation-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DPROJECT=${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "-DOUTPUT=${CMAKE_BINARY_DIR}/cli-marker-validation-contract"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_marker_validation_contract_smoke.cmake"
)
add_test(
    NAME wave-cli-actionable-validation-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DPROJECT=${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "-DOUTPUT=${CMAKE_BINARY_DIR}/cli-actionable-validation-contract"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_actionable_validation_contract_smoke.cmake"
)
add_test(
    NAME wave-cli-relation-validation-context-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DPROJECT=${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "-DOUTPUT=${CMAKE_BINARY_DIR}/cli-relation-validation-context-contract"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_relation_validation_context_contract_smoke.cmake"
)
add_test(
    NAME wave-cli-waveform-validation-context-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DPROJECT=${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "-DOUTPUT=${CMAKE_BINARY_DIR}/cli-waveform-validation-context-contract"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_waveform_validation_context_contract_smoke.cmake"
)
add_test(
    NAME wave-cli-event-validation-context-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DPROJECT=${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "-DOUTPUT=${CMAKE_BINARY_DIR}/cli-event-validation-context-contract"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_event_validation_context_contract_smoke.cmake"
)
add_test(
    NAME wave-cli-event-link-repair-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DPROJECT=${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "-DOUTPUT=${CMAKE_BINARY_DIR}/cli-event-link-repair-contract"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_event_link_repair_contract_smoke.cmake"
)
add_test(
    NAME wave-cli-event-cycle-repair-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DPROJECT=${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "-DOUTPUT=${CMAKE_BINARY_DIR}/cli-event-cycle-repair-contract"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_event_cycle_repair_contract_smoke.cmake"
)
add_test(
    NAME wave-cli-event-clock-repair-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DPROJECT=${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "-DOUTPUT=${CMAKE_BINARY_DIR}/cli-event-clock-repair-contract"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_event_clock_repair_contract_smoke.cmake"
)
add_test(
    NAME wave-cli-lane-clock-repair-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DPROJECT=${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "-DOUTPUT=${CMAKE_BINARY_DIR}/cli-lane-clock-repair-contract"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_lane_clock_repair_contract_smoke.cmake"
)
add_test(
    NAME wave-cli-lane-group-repair-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DPROJECT=${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "-DOUTPUT=${CMAKE_BINARY_DIR}/cli-lane-group-repair-contract"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_lane_group_repair_contract_smoke.cmake"
)
add_test(
    NAME wave-cli-relation-clock-repair-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DPROJECT=${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "-DOUTPUT=${CMAKE_BINARY_DIR}/cli-relation-clock-repair-contract"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_relation_clock_repair_contract_smoke.cmake"
)
add_test(
    NAME wave-cli-trace-mapping-repair-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DPROJECT=${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "-DOUTPUT=${CMAKE_BINARY_DIR}/cli-trace-mapping-repair-contract"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_trace_mapping_repair_contract_smoke.cmake"
)
add_test(
    NAME wave-cli-trace-identity-compare-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DWAVE_COMPARE=$<TARGET_FILE:wave-compare>"
            "-DPROJECT=${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "-DOUTPUT=${CMAKE_BINARY_DIR}/cli-trace-identity-compare-contract"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_trace_identity_compare_contract_smoke.cmake"
)
add_test(
    NAME wave-cli-trace-reference-repair-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DWAVE_COMPARE=$<TARGET_FILE:wave-compare>"
            "-DPROJECT=${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "-DOUTPUT=${CMAKE_BINARY_DIR}/cli-trace-reference-repair-contract"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_trace_reference_repair_contract_smoke.cmake"
)
add_test(
    NAME wave-cli-scenario-selection-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DWAVE_GENERATE=$<TARGET_FILE:wave-generate>"
            "-DWAVE_COMPARE=$<TARGET_FILE:wave-compare>"
            "-DWAVE_BRIDGE=$<TARGET_FILE:wave-bridge>"
            "-DPROJECT=${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "-DSIGNALS=${CMAKE_SOURCE_DIR}/examples/handshake/integration/zeroslack_signals.json"
            "-DARTIFACT_DIR=${CMAKE_SOURCE_DIR}/examples/handshake/exports"
            "-DOUTPUT=${CMAKE_BINARY_DIR}/cli-scenario-selection-contract"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_scenario_selection_contract_smoke.cmake"
)
add_test(
    NAME wave-cli-compatibility-dispatch-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_CLI=$<TARGET_FILE:wave-cli>"
            "-DPROJECT=${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
            "-DOPERATIONS=${CMAKE_SOURCE_DIR}/docs/examples/quick-start.operations.json"
            "-DOUTPUT=${CMAKE_BINARY_DIR}/cli-compatibility-dispatch"
            -P "${CMAKE_SOURCE_DIR}/tests/cli_compatibility_dispatch_smoke.cmake"
)
add_test(
    NAME wave-toolchain-probe-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_SIM_RUNNER=$<TARGET_FILE:wave-sim-runner>"
            "-DREADY_FIXTURE=$<TARGET_FILE:wave-toolchain-fixture-ready>"
            "-DOLD_FIXTURE=$<TARGET_FILE:wave-toolchain-fixture-old>"
            "-DFAILURE_FIXTURE=$<TARGET_FILE:wave-toolchain-fixture-failure>"
            "-DHANG_FIXTURE=$<TARGET_FILE:wave-toolchain-fixture-hang>"
            "-DOUTPUT=${CMAKE_BINARY_DIR}/toolchain-probe-contract"
            -P "${CMAKE_SOURCE_DIR}/tests/toolchain_probe_cli_smoke.cmake"
)
set(_wave_fixed_fixture_root
    "${CMAKE_SOURCE_DIR}/tests/fixtures/simulation/fixed-counter")
add_test(
    NAME wave-fixed-fixture-cli-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DWAVE_SIM_RUNNER=$<TARGET_FILE:wave-sim-runner>"
            "-DVERILATOR_FIXTURE=$<TARGET_FILE:wave-verilator-fixture>"
            "-DCXX_FIXTURE=$<TARGET_FILE:wave-toolchain-fixture-ready>"
            "-DSIMULATOR_FIXTURE=$<TARGET_FILE:wave-simulator-fixture>"
            "-DFIXTURE_ROOT=${_wave_fixed_fixture_root}"
            "-DOUTPUT=${CMAKE_BINARY_DIR}/fixed-fixture-cli"
            -P "${CMAKE_SOURCE_DIR}/tests/simulation_fixture_cli_smoke.cmake"
)
add_test(
    NAME wave-simulation-controls-smoke
    COMMAND Tickx
            --load-first-trace
            --simulation-controls-smoke
            "${CMAKE_BINARY_DIR}/fixed-fixture-cli/result.wave.json"
)
add_test(
    NAME wave-simulation-stop-smoke
    COMMAND Tickx
            --load-first-trace
            --simulation-stop-smoke
            "${CMAKE_BINARY_DIR}/fixed-fixture-cli/result.wave.json"
)
set_tests_properties(
    wave-simulation-controls-smoke wave-simulation-stop-smoke
    PROPERTIES
        DEPENDS wave-fixed-fixture-cli-smoke
        TIMEOUT 20
)
add_test(
    NAME wave-fixed-fixture-ui-smoke
    COMMAND wave-fixed-fixture-ui-smoke
            "${CMAKE_BINARY_DIR}/fixed-fixture-ui.png"
)
find_program(WAVE_TEST_VERILATOR_EXECUTABLE NAMES verilator verilator_bin)
if(WAVE_TEST_VERILATOR_EXECUTABLE)
    add_test(
        NAME wave-fixed-fixture-real-verilator
        COMMAND "${CMAKE_COMMAND}"
                "-DWAVE_SIM_RUNNER=$<TARGET_FILE:wave-sim-runner>"
                "-DVERILATOR=${WAVE_TEST_VERILATOR_EXECUTABLE}"
                "-DCXX=${CMAKE_CXX_COMPILER}"
                "-DFIXTURE_ROOT=${_wave_fixed_fixture_root}"
                "-DOUTPUT=${CMAKE_BINARY_DIR}/fixed-fixture-real"
                -P "${CMAKE_SOURCE_DIR}/tests/simulation_fixture_real_smoke.cmake"
    )
else()
    message(STATUS "Verilator not found; real fixed-fixture CTest is not registered")
endif()
add_test(
    NAME wave-portable-install-contract-smoke
    COMMAND "${CMAKE_COMMAND}"
            "-DBUILD_DIRECTORY=${CMAKE_BINARY_DIR}"
            "-DCONFIGURATION=$<CONFIG>"
            "-DOUTPUT=${CMAKE_BINARY_DIR}/portable-install-contract"
            -P "${CMAKE_SOURCE_DIR}/tests/portable_install_contract_smoke.cmake"
)
if(WIN32)
    add_test(
        NAME wave-cli-powershell-stdin-smoke
        COMMAND powershell.exe -NoProfile -NonInteractive
                -ExecutionPolicy Bypass
                -File "${CMAKE_SOURCE_DIR}/tests/cli_powershell_stdin_smoke.ps1"
                -WaveCli "$<TARGET_FILE:wave-cli>"
                -Project "${CMAKE_SOURCE_DIR}/examples/handshake/project.wave.json"
                -Operations "${CMAKE_SOURCE_DIR}/tests/fixtures/cli-operations.json"
    )
endif()

get_target_property(_wave_qt_core_location Qt6::Core IMPORTED_LOCATION)
get_filename_component(_wave_qt_bin_dir "${_wave_qt_core_location}" DIRECTORY)
get_filename_component(_wave_compiler_bin_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
set(_wave_test_environment "QT_QPA_PLATFORM=offscreen")
if(WIN32 AND EXISTS "C:/Windows/Fonts/segoeui.ttf")
    set(_wave_test_font_dir "${CMAKE_BINARY_DIR}/test-fonts")
    file(MAKE_DIRECTORY "${_wave_test_font_dir}")
    foreach(_wave_test_font IN ITEMS segoeui.ttf segoeuib.ttf segoeuii.ttf msyh.ttc msyhbd.ttc)
        if(EXISTS "C:/Windows/Fonts/${_wave_test_font}")
            configure_file(
                "C:/Windows/Fonts/${_wave_test_font}"
                "${_wave_test_font_dir}/${_wave_test_font}"
                COPYONLY)
        endif()
    endforeach()
    list(APPEND _wave_test_environment
        "QT_QPA_FONTDIR=${_wave_test_font_dir}")
endif()
set_property(TEST wave-canvas-edit-regression-test APPEND PROPERTY ENVIRONMENT
    "QT_QPA_FONTDIR=${_wave_test_font_dir}")
foreach(_wave_ela_scale IN ITEMS 1 1.25 1.5 2)
    set_property(TEST wave-ela-ui-${_wave_ela_scale} APPEND PROPERTY ENVIRONMENT
        "QT_QPA_FONTDIR=${_wave_test_font_dir}")
    set_property(TEST wave-ui-followup-${_wave_ela_scale} APPEND PROPERTY ENVIRONMENT
        "QT_QPA_FONTDIR=${_wave_test_font_dir}")
    if(WIN32)
        set_property(TEST wave-ui-native-${_wave_ela_scale} APPEND PROPERTY ENVIRONMENT
            "QT_QPA_FONTDIR=${_wave_test_font_dir}")
    endif()
endforeach()
foreach(_waveform_scale_test _waveform_scale IN ZIP_LISTS
        _waveform_view_scale_tests _waveform_view_scales)
    set_tests_properties(${_waveform_scale_test} PROPERTIES
        ENVIRONMENT "${_wave_test_environment};QT_SCALE_FACTOR=${_waveform_scale}"
        ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${CMAKE_BINARY_DIR};PATH=path_list_prepend:${_wave_qt_bin_dir};PATH=path_list_prepend:${_wave_compiler_bin_dir}"
        TIMEOUT 20)
endforeach()
set_tests_properties(
    wave-core-tests wave-timeline-viewport-test wavewidgets-abi-test wave-app-smoke wave-example-smoke wave-simulation-result-window-smoke wave-simulation-controls-smoke wave-simulation-stop-smoke wave-autosave-smoke wave-file-conflict-smoke wave-lane-dialog-smoke wave-lane-removal-smoke wave-lane-reorder-smoke wave-lane-autoscroll-smoke wave-hidden-lane-smoke wave-group-header-smoke wave-group-creation-smoke wave-group-batch-smoke wave-group-membership-smoke wave-signal-header-smoke wave-canvas-add-lane-smoke wave-cursor-mode-smoke wave-wave-edit-smoke wave-wave-edit-autoscroll-smoke wave-new-project-smoke wave-user-journey-smoke wave-edit-menu-smoke wave-waveform-only-smoke wave-uri-smoke wave-uri-invalid-id-smoke wave-scenario-switch-smoke wave-generate-smoke wave-compare-smoke
    wave-bridge-describe-smoke wave-bridge-signals-smoke wave-bridge-frame-smoke
    wave-bridge-module-manifest-smoke wave-bridge-module-manifest-load-smoke wave-bridge-pinloom-smoke wave-cli-inspect-smoke wave-cli-validate-smoke
    wave-bridge-stimulus-export-smoke wave-bridge-stimulus-import-smoke
    wave-bridge-stimulus-load-smoke wave-bridge-stimulus-validate-smoke
    wave-cli-summary-smoke wave-cli-sample-smoke wave-cli-window-smoke
    wave-cli-apply-dry-run-smoke wave-cli-apply-smoke wave-cli-contract-smoke
    wave-cli-advanced-contract-smoke wave-cli-window-range-contract-smoke
    wave-cli-new-dry-run-smoke wave-cli-new-sequence-contract-smoke
    wave-cli-signals-smoke wave-cli-selector-contract-smoke
    wave-cli-symbolic-contract-smoke wave-cli-intent-contract-smoke
    wave-cli-capabilities-group-contract-smoke
    wave-cli-duplicate-signal-contract-smoke
    wave-cli-duration-contract-smoke
    wave-cli-edges-contract-smoke
    wave-cli-markers-contract-smoke
    wave-cli-relations-contract-smoke
    wave-cli-relation-repair-contract-smoke
    wave-cli-relation-identity-repair-contract-smoke
    wave-cli-identity-validation-contract-smoke
    wave-cli-post-validation-guard-contract-smoke
    wave-cli-marker-repair-contract-smoke
    wave-cli-marker-validation-contract-smoke
    wave-cli-actionable-validation-contract-smoke
    wave-cli-relation-validation-context-contract-smoke
    wave-cli-waveform-validation-context-contract-smoke
    wave-cli-event-validation-context-contract-smoke
    wave-cli-event-link-repair-contract-smoke
    wave-cli-event-cycle-repair-contract-smoke
    wave-cli-event-clock-repair-contract-smoke
    wave-cli-lane-clock-repair-contract-smoke
    wave-cli-lane-group-repair-contract-smoke
    wave-cli-relation-clock-repair-contract-smoke
    wave-cli-trace-mapping-repair-contract-smoke
    wave-cli-trace-identity-compare-contract-smoke
    wave-cli-trace-reference-repair-contract-smoke
    wave-cli-scenario-selection-contract-smoke
    wave-cli-compatibility-dispatch-smoke
    wave-toolchain-probe-contract-smoke
    wave-portable-install-contract-smoke
    PROPERTIES
    ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${CMAKE_BINARY_DIR};PATH=path_list_prepend:${_wave_qt_bin_dir};PATH=path_list_prepend:${_wave_compiler_bin_dir}"
)
set_tests_properties(
    wave-core-tests wave-timeline-viewport-test wavewidgets-abi-test wave-app-smoke wave-example-smoke wave-simulation-result-window-smoke wave-simulation-controls-smoke wave-simulation-stop-smoke wave-autosave-smoke wave-file-conflict-smoke wave-lane-dialog-smoke wave-lane-removal-smoke wave-lane-reorder-smoke wave-lane-autoscroll-smoke wave-hidden-lane-smoke wave-group-header-smoke wave-group-creation-smoke wave-group-batch-smoke wave-group-membership-smoke wave-signal-header-smoke wave-canvas-add-lane-smoke wave-cursor-mode-smoke wave-wave-edit-smoke wave-wave-edit-autoscroll-smoke wave-new-project-smoke wave-user-journey-smoke wave-edit-menu-smoke wave-waveform-only-smoke wave-uri-smoke wave-uri-invalid-id-smoke wave-scenario-switch-smoke wave-generate-smoke wave-compare-smoke
    wave-bridge-describe-smoke wave-bridge-signals-smoke wave-bridge-frame-smoke
    wave-bridge-module-manifest-smoke wave-bridge-module-manifest-load-smoke wave-bridge-pinloom-smoke wave-cli-inspect-smoke wave-cli-validate-smoke
    wave-bridge-stimulus-export-smoke wave-bridge-stimulus-import-smoke
    wave-bridge-stimulus-load-smoke wave-bridge-stimulus-validate-smoke
    wave-cli-summary-smoke wave-cli-sample-smoke wave-cli-window-smoke
    wave-cli-apply-dry-run-smoke wave-cli-apply-smoke wave-cli-contract-smoke
    wave-cli-advanced-contract-smoke wave-cli-window-range-contract-smoke
    wave-cli-new-dry-run-smoke wave-cli-new-sequence-contract-smoke
    wave-cli-signals-smoke wave-cli-selector-contract-smoke
    wave-cli-symbolic-contract-smoke wave-cli-intent-contract-smoke
    wave-cli-capabilities-group-contract-smoke
    wave-cli-duplicate-signal-contract-smoke
    wave-cli-duration-contract-smoke
    wave-cli-edges-contract-smoke
    wave-cli-markers-contract-smoke
    wave-cli-relations-contract-smoke
    wave-cli-relation-repair-contract-smoke
    wave-cli-relation-identity-repair-contract-smoke
    wave-cli-identity-validation-contract-smoke
    wave-cli-post-validation-guard-contract-smoke
    wave-cli-marker-repair-contract-smoke
    wave-cli-marker-validation-contract-smoke
    wave-cli-actionable-validation-contract-smoke
    wave-cli-relation-validation-context-contract-smoke
    wave-cli-waveform-validation-context-contract-smoke
    wave-cli-event-validation-context-contract-smoke
    wave-cli-event-link-repair-contract-smoke
    wave-cli-event-cycle-repair-contract-smoke
    wave-cli-event-clock-repair-contract-smoke
    wave-cli-lane-clock-repair-contract-smoke
    wave-cli-lane-group-repair-contract-smoke
    wave-cli-relation-clock-repair-contract-smoke
    wave-cli-trace-mapping-repair-contract-smoke
    wave-cli-trace-identity-compare-contract-smoke
    wave-cli-trace-reference-repair-contract-smoke
    wave-cli-scenario-selection-contract-smoke
    wave-cli-compatibility-dispatch-smoke
    wave-toolchain-probe-contract-smoke
    wave-portable-install-contract-smoke
    PROPERTIES
    ENVIRONMENT "${_wave_test_environment}"
)
set_tests_properties(wave-external-reload-ui-smoke PROPERTIES
    ENVIRONMENT "${_wave_test_environment}"
    ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${CMAKE_BINARY_DIR};PATH=path_list_prepend:${_wave_qt_bin_dir};PATH=path_list_prepend:${_wave_compiler_bin_dir}"
    TIMEOUT 30
)
set_tests_properties(wave-large-canvas-benchmark PROPERTIES
    ENVIRONMENT "${_wave_test_environment}"
    ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${CMAKE_BINARY_DIR};PATH=path_list_prepend:${_wave_qt_bin_dir};PATH=path_list_prepend:${_wave_compiler_bin_dir}"
    TIMEOUT 60
)
set_tests_properties(wave-relation-edit-ui-smoke PROPERTIES
    ENVIRONMENT "${_wave_test_environment}"
    ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${CMAKE_BINARY_DIR};PATH=path_list_prepend:${_wave_qt_bin_dir};PATH=path_list_prepend:${_wave_compiler_bin_dir}"
    TIMEOUT 30
)
foreach(_wave_gui_test IN ITEMS
    wave-app-smoke wave-example-smoke wave-simulation-result-window-smoke wave-simulation-controls-smoke wave-simulation-stop-smoke wave-autosave-smoke wave-file-conflict-smoke wave-lane-dialog-smoke
    wave-lane-removal-smoke wave-lane-reorder-smoke wave-lane-autoscroll-smoke wave-hidden-lane-smoke wave-group-header-smoke wave-group-creation-smoke wave-group-batch-smoke wave-group-membership-smoke wave-signal-header-smoke wave-canvas-add-lane-smoke
    wave-cursor-mode-smoke wave-wave-edit-smoke wave-wave-edit-autoscroll-smoke wave-new-project-smoke
    wave-user-journey-smoke wave-edit-menu-smoke wave-waveform-only-smoke wave-uri-smoke wave-uri-invalid-id-smoke
    wave-scenario-switch-smoke
)
    set_property(TEST ${_wave_gui_test} APPEND PROPERTY ENVIRONMENT
        "WAVEWORKBENCH_RECOVERY_DIR=${CMAKE_BINARY_DIR}/test-recovery/${_wave_gui_test}"
        "WAVEWORKBENCH_SETTINGS_DIR=${CMAKE_BINARY_DIR}/test-settings/${_wave_gui_test}")
endforeach()
set_tests_properties(wave-app-smoke wave-example-smoke wave-simulation-result-window-smoke wave-simulation-controls-smoke wave-simulation-stop-smoke wave-autosave-smoke wave-file-conflict-smoke wave-lane-dialog-smoke wave-lane-removal-smoke wave-lane-reorder-smoke wave-lane-autoscroll-smoke wave-hidden-lane-smoke wave-group-header-smoke wave-group-creation-smoke wave-group-batch-smoke wave-group-membership-smoke wave-signal-header-smoke wave-canvas-add-lane-smoke wave-cursor-mode-smoke wave-wave-edit-smoke wave-wave-edit-autoscroll-smoke wave-new-project-smoke wave-user-journey-smoke wave-edit-menu-smoke wave-waveform-only-smoke wave-uri-smoke wave-uri-invalid-id-smoke wave-scenario-switch-smoke PROPERTIES
    TIMEOUT 20
)
if(TEST wave-fst-on-demand-ui-smoke)
    set_tests_properties(wave-fst-on-demand-ui-smoke PROPERTIES
        ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${CMAKE_BINARY_DIR};PATH=path_list_prepend:${_wave_qt_bin_dir};PATH=path_list_prepend:${_wave_compiler_bin_dir}"
        ENVIRONMENT "${_wave_test_environment}"
        TIMEOUT 20)
    set_property(TEST wave-fst-on-demand-ui-smoke APPEND PROPERTY ENVIRONMENT
        "WAVEWORKBENCH_RECOVERY_DIR=${CMAKE_BINARY_DIR}/test-recovery/wave-fst-on-demand-ui-smoke"
        "WAVEWORKBENCH_SETTINGS_DIR=${CMAKE_BINARY_DIR}/test-settings/wave-fst-on-demand-ui-smoke")
endif()
set_property(TEST wave-simulation-controls-smoke APPEND PROPERTY ENVIRONMENT
    "WAVE_SIMULATOR_FIXTURE=$<TARGET_FILE:wave-simulator-fixture>")
set_property(TEST wave-simulation-stop-smoke APPEND PROPERTY ENVIRONMENT
    "WAVE_SIMULATOR_FIXTURE=$<TARGET_FILE:wave-toolchain-fixture-hang>")
set(_wave_fixed_fixture_tests
    wave-fixed-fixture-cli-smoke
    wave-fixed-fixture-ui-smoke
    wave-expected-actual-ui-smoke
    wave-structured-input-ui-smoke
    wave-stub-selection-ui-smoke)
list(APPEND _wave_fixed_fixture_tests wave-multi-scenario-batch-ui-smoke)
list(APPEND _wave_fixed_fixture_tests wave-source-navigation-ui-smoke)
if(TEST wave-fixed-fixture-real-verilator)
    list(APPEND _wave_fixed_fixture_tests wave-fixed-fixture-real-verilator)
endif()
set_tests_properties(${_wave_fixed_fixture_tests} PROPERTIES
    ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${CMAKE_BINARY_DIR};PATH=path_list_prepend:${_wave_qt_bin_dir};PATH=path_list_prepend:${_wave_compiler_bin_dir}"
    ENVIRONMENT "${_wave_test_environment}"
    TIMEOUT 180
)
set_property(TEST wave-multi-scenario-batch-ui-smoke APPEND PROPERTY ENVIRONMENT
    "WAVEWORKBENCH_RECOVERY_DIR=${CMAKE_BINARY_DIR}/test-recovery/wave-multi-scenario-batch-ui-smoke"
    "WAVEWORKBENCH_SETTINGS_DIR=${CMAKE_BINARY_DIR}/test-settings/wave-multi-scenario-batch-ui-smoke")
set_property(TEST wave-source-navigation-ui-smoke APPEND PROPERTY ENVIRONMENT
    "WAVEWORKBENCH_RECOVERY_DIR=${CMAKE_BINARY_DIR}/test-recovery/wave-source-navigation-ui-smoke"
    "WAVEWORKBENCH_SETTINGS_DIR=${CMAKE_BINARY_DIR}/test-settings/wave-source-navigation-ui-smoke")
if(WIN32)
    set_tests_properties(
        wave-cli-powershell-stdin-smoke
        PROPERTIES
        ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${_wave_qt_bin_dir};PATH=path_list_prepend:${_wave_compiler_bin_dir}"
        ENVIRONMENT "${_wave_test_environment}"
    )
endif()

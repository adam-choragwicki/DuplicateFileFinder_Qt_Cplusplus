add_executable(scan_workflow_tests
        scan_test_fixtures.cpp scan_test_fixtures.h
        shared_scan_workflow_test.cpp
        scan_by_file_name_workflow_test.cpp
        scan_by_file_content_workflow_test.cpp
        scan_summary_logger_test.cpp
        unreadable_file_scan_test_helpers.cpp unreadable_file_scan_test_helpers.h
)

target_link_libraries(scan_workflow_tests
        PRIVATE
        Qt6::Core
        GTest::gtest_main
        app_backend
)

target_compile_definitions(scan_workflow_tests PRIVATE
        FILE_SYSTEM_SCENARIOS_DIRECTORY="${PROJECT_SOURCE_DIR}/tests/file_system_scenarios"
)

include(GoogleTest)
gtest_add_tests(TARGET scan_workflow_tests TEST_LIST scanWorkflowTests)

if (WIN32)
    # CTest needs the Qt DLL directory on PATH when it launches the scan workflow tests.
    set_tests_properties(${scanWorkflowTests} PROPERTIES
            ENVIRONMENT "PATH=$<TARGET_FILE_DIR:Qt6::Core>\;$ENV{PATH}"
    )
endif ()

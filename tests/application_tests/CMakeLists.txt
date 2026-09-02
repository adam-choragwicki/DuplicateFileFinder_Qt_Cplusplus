add_executable(application_tests
        application_test_main.cpp
        html_result_exporter_test.cpp
        asynchronous_scanner_test.cpp
        model_scan_root_test.cpp
        scan_integration_test.cpp
        temporary_scan_directory_test_fixture.h

        ${PROJECT_SOURCE_DIR}/src/controller.cpp ${PROJECT_SOURCE_DIR}/src/controller.h
        ${PROJECT_SOURCE_DIR}/src/reporting/html_result_exporter.cpp ${PROJECT_SOURCE_DIR}/src/reporting/html_result_exporter.h
)

target_link_libraries(application_tests
        PRIVATE
        GTest::gtest
        Qt6::Widgets
        app_backend
        app_frontend
        test_helpers
)

include(GoogleTest)
gtest_add_tests(TARGET application_tests TEST_LIST applicationTests)

set_tests_properties(${applicationTests} PROPERTIES TIMEOUT 15)

if (WIN32)
    # CTest needs the Qt DLL directory on PATH when it launches the application tests.
    set_tests_properties(${applicationTests} PROPERTIES
            ENVIRONMENT "PATH=$<TARGET_FILE_DIR:Qt6::Widgets>\;$ENV{PATH}"
    )
endif ()

add_executable(frontend_tests
        frontend_test_main.cpp
        main_window_test.cpp
        results_tab_test.cpp
)

target_link_libraries(frontend_tests
        PRIVATE
        GTest::gtest
        app_frontend
)

include(GoogleTest)
gtest_add_tests(TARGET frontend_tests TEST_LIST frontendTests)

if (WIN32)
    # CTest needs the Qt DLL directory on PATH when it launches the frontend tests.
    set_tests_properties(${frontendTests} PROPERTIES
            ENVIRONMENT "PATH=$<TARGET_FILE_DIR:Qt6::Widgets>\;$ENV{PATH}"
    )
endif ()

add_subdirectory(types)

qt_add_executable(${PROJECT_NAME}
        main.cpp
        application.cpp application.h
        model.cpp model.h model.cpp
        view/main_window.cpp view/main_window.h view/main_window.ui
        view/results_tab.cpp view/results_tab.h view/results_tab.ui
        controller.cpp controller.h
        log_manager.cpp log_manager.h
        scanner.cpp scanner.h
        scan_request.h
        scan_progress.h
        scan_workflow.h
        file_name_scan_workflow.h file_name_scan_workflow.cpp
        file_content_scan_workflow.h file_content_scan_workflow.cpp
        file_collector.cpp file_collector.h
        file_hasher.h
        html_result_exporter.cpp html_result_exporter.h

        scan_summary/scan_summary.h
        scan_summary/scan_summary_logger.h
        scan_summary/file_name_scan_summary.h
        scan_summary/file_content_scan_summary.h
)

target_include_directories(${PROJECT_NAME} PRIVATE .)

target_link_libraries(${PROJECT_NAME}
        PRIVATE
        Qt6::Widgets
        Qt6::Concurrent
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
        APP_VERSION="${PROJECT_VERSION}"
        BUILD_TYPE="$<CONFIG>"
)

if (WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE $<CONFIG:Release>) # this avoids opening console window on Windows in Release mode
endif ()

set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)

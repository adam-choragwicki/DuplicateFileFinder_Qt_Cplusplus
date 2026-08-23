add_subdirectory(types)
add_subdirectory(view)

add_library(scan_core
        file_collector.cpp file_collector.h
        scan_workflow.h
        file_name_scan_workflow.cpp file_name_scan_workflow.h
        file_content_scan_workflow.cpp file_content_scan_workflow.h
        file_comparator.cpp file_comparator.h
        file_hasher.h
        scan_summary/scan_summary_logger.h
)

target_include_directories(scan_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(scan_core
        PUBLIC
        Qt6::Core
        types
)

qt_add_executable(${PROJECT_NAME}
        main.cpp
        application.cpp application.h
        model.cpp model.h model.cpp
        controller.cpp controller.h
        log_manager.cpp log_manager.h
        scanner.cpp scanner.h
        scan_request.h
        scan_progress.h
        html_result_exporter.cpp html_result_exporter.h

        scan_summary/scan_summary.h
        scan_summary/file_name_scan_summary.h
        scan_summary/file_content_scan_summary.h
)

target_include_directories(${PROJECT_NAME} PRIVATE .)

target_link_libraries(${PROJECT_NAME}
        PRIVATE
        Qt6::Widgets
        Qt6::Concurrent
        scan_core
        app_frontend
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
        APP_VERSION="${PROJECT_VERSION}"
        BUILD_TYPE="$<CONFIG>"
)

if (WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE $<CONFIG:Release>) # this avoids opening console window on Windows in Release mode
endif ()

set_target_properties(${PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)

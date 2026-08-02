add_subdirectory(types)

qt_add_executable(${PROJECT_NAME}
        main.cpp
        application.cpp application.h
        model.cpp model.h model.cpp
        main_window.cpp main_window.h main_window.ui
        controller.cpp controller.h
        log_manager.cpp log_manager.h
        scanner.cpp scanner.h
        scan_request.h
        scan_workflow.h
        file_name_scan_workflow.h file_name_scan_workflow.cpp
        file_content_scan_workflow.h file_content_scan_workflow.cpp
)

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

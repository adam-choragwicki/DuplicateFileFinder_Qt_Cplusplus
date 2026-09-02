add_subdirectory(backend)
add_subdirectory(frontend)

qt_add_executable(${PROJECT_NAME}
        main.cpp
        application.cpp application.h
        controller.cpp controller.h
        log_manager.cpp log_manager.h
        reporting/html_result_exporter.cpp reporting/html_result_exporter.h
        development/development_configuration_helper.cpp development/development_configuration_helper.h
)

target_include_directories(${PROJECT_NAME} PRIVATE .)

target_link_libraries(${PROJECT_NAME}
        PRIVATE
        app_backend
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

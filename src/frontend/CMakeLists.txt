add_library(app_frontend
        main_window.cpp main_window.h main_window.ui
        results_tab.cpp results_tab.h results_tab.ui
        scan_directories_tree_model.cpp scan_directories_tree_model.h
        duplicate_results_table_model.cpp duplicate_results_table_model.h
        file_size_formatter.h
)

target_include_directories(app_frontend
        PUBLIC
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/src/backend
)

target_link_libraries(app_frontend
        PUBLIC
        Qt6::Widgets
)

target_compile_definitions(app_frontend PRIVATE APP_VERSION="${PROJECT_VERSION}")

add_library(app_frontend
        main_window.cpp main_window.h main_window.ui
        results_tab.cpp results_tab.h results_tab.ui
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

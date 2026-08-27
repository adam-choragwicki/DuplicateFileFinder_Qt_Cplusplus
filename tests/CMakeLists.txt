if (MSVC)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
endif ()

set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
add_subdirectory(lib/googletest EXCLUDE_FROM_ALL)

# Header-only target shared by multiple test executables to make IDE recognize the helper headers as project files.
# the target only propagates its listed sources and include-directory usage requirements to targets that link against it.
add_library(test_helpers INTERFACE)

target_sources(test_helpers INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/test_helpers/scan_result_test_helpers.h
)

target_include_directories(test_helpers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(application_tests)
add_subdirectory(scan_workflow_tests)
add_subdirectory(frontend_tests)

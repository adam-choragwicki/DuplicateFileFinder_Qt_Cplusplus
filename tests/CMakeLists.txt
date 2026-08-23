if (MSVC)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
endif ()

set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
add_subdirectory(lib/googletest EXCLUDE_FROM_ALL)

add_subdirectory(scan_workflow_tests)
add_subdirectory(frontend_tests)

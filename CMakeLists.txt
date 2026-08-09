cmake_minimum_required(VERSION 3.16)

project(DuplicateFileFinder VERSION 1.0 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS Widgets Concurrent)
include(CTest)

add_subdirectory(src)

if (BUILD_TESTING)
    add_subdirectory(tests)
endif ()

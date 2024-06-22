cmake_minimum_required(VERSION 3.20 FATAL_ERROR)

project(FlexSched VERSION 0.0.1)


if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()


set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/Modules/")


option(enable_warnings "Enable compilation warnings" ON)
option(treat_warnings_as_errors "Treat compilation warnings as compilation errors" OFF)
option(do_coverage "Compile in coverage mode" OFF)


include_directories("inc" "${PROJECT_BINARY_DIR}")


add_subdirectory(src)

set(LIBRARY_LINKAGE "SHARED" CACHE STRING "Library linkage.")
set_property(CACHE LIBRARY_LINKAGE PROPERTY STRINGS "SHARED" "STATIC")


if(do_coverage)
    include(CodeCoverage)
    append_coverage_compiler_flags()
endif()


if(enable_warnings)
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra)
endif()

if(treat_warnings_as_errors)
    target_compile_options(${PROJECT_NAME} PRIVATE -Werror)
endif()
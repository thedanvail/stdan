cmake_minimum_required(VERSION 3.16)
project(stdan CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(STDAN_BUILD_TESTS "Build stdan test targets" OFF)

file(GLOB_RECURSE STDAN_SOURCES CONFIGURE_DEPENDS src/*.cpp)
list(FILTER STDAN_SOURCES EXCLUDE REGEX "src/main\\.cpp$")

if(STDAN_SOURCES)
    add_library(stdan STATIC ${STDAN_SOURCES})
    target_include_directories(stdan PUBLIC include)
    target_compile_definitions(stdan PUBLIC
        $<$<CONFIG:Debug>:STDAN_DEBUG=1>
        $<$<NOT:$<CONFIG:Debug>>:STDAN_DEBUG=0>
    )
else()
    add_library(stdan INTERFACE)
    target_include_directories(stdan INTERFACE include)
    target_compile_definitions(stdan INTERFACE
        $<$<CONFIG:Debug>:STDAN_DEBUG=1>
        $<$<NOT:$<CONFIG:Debug>>:STDAN_DEBUG=0>
    )

    # Keep compile_commands.json useful for editors in header-only mode.
    set(STDAN_TOOLING_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/ps_vector.cpp")
    file(WRITE "${STDAN_TOOLING_SOURCE}" [[#include "storage/ps_vector.hpp"
]])
    add_library(stdan_tooling OBJECT EXCLUDE_FROM_ALL "${STDAN_TOOLING_SOURCE}")
    target_link_libraries(stdan_tooling PRIVATE stdan)
endif()

if(STDAN_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

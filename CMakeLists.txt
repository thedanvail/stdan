cmake_minimum_required(VERSION 3.16)
project(stdan CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB_RECURSE LIB_SOURCES CONFIGURE_DEPENDS src/*.cpp)
list(FILTER LIB_SOURCES EXCLUDE REGEX "src/main\\.cpp$")

if(NOT LIB_SOURCES)
    set(STDAN_DUMMY_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/stdan_dummy.cpp")
    file(WRITE "${STDAN_DUMMY_SOURCE}" "// Dummy translation unit for static library build.\n")
    list(APPEND LIB_SOURCES "${STDAN_DUMMY_SOURCE}")
endif()

add_library(stdan STATIC ${LIB_SOURCES})
target_include_directories(stdan PUBLIC include)
target_compile_definitions(stdan PUBLIC
    $<$<CONFIG:Debug>:DAN_DEBUG=1>
    $<$<NOT:$<CONFIG:Debug>>:DAN_DEBUG=0>
)

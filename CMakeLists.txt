cmake_minimum_required(VERSION 3.16)

project(stdan CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(STDAN_BUILD_TESTS "Build stdan test targets" OFF)
option(STDAN_USE_LIBCXX "Use libc++ when compiling with Clang" OFF)

if(STDAN_USE_LIBCXX)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "STDAN_USE_LIBCXX requires Clang")
    endif()

    add_compile_options(-stdlib=libc++)
    add_link_options(-stdlib=libc++)
endif()

message(STATUS "CXX compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "CXX compiler path: ${CMAKE_CXX_COMPILER}")

file(GLOB_RECURSE STDAN_SOURCES CONFIGURE_DEPENDS src/*.cpp)

add_library(stdan STATIC ${STDAN_SOURCES})
target_include_directories(stdan PUBLIC include)
target_compile_definitions(stdan PUBLIC
    $<$<CONFIG:Debug>:STDAN_DEBUG=1>
    $<$<NOT:$<CONFIG:Debug>>:STDAN_DEBUG=0>
)

if(STDAN_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

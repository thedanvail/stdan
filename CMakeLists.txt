cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED CMAKE_CXX_COMPILER)
    set(CMAKE_CXX_COMPILER g++)
endif()

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
list(FILTER STDAN_SOURCES EXCLUDE REGEX "src/main\\.cpp$")
file(GLOB_RECURSE STDAN_HEADERS CONFIGURE_DEPENDS include/*.hpp)
set(STDAN_HEADER_DIRS)
foreach(header IN LISTS STDAN_HEADERS)
    get_filename_component(dir ${header} DIRECTORY)
    list(APPEND STDAN_HEADER_DIRS ${dir})
endforeach()
list(REMOVE_DUPLICATES STDAN_HEADER_DIRS)

add_library(stdan STATIC ${STDAN_SOURCES})
target_include_directories(stdan PUBLIC include ${STDAN_HEADER_DIRS})
target_sources(stdan PRIVATE ${STDAN_HEADERS})
target_compile_definitions(stdan PUBLIC
    $<$<CONFIG:Debug>:STDAN_DEBUG=1>
    $<$<NOT:$<CONFIG:Debug>>:STDAN_DEBUG=0>
)

if(STDAN_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

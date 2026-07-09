cmake_minimum_required(VERSION 3.16)

project(stdan CXX)

include(CheckCXXSourceCompiles)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(STDAN_BUILD_TESTS "Build stdan test targets" OFF)

message(STATUS "CXX compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "CXX compiler path: ${CMAKE_CXX_COMPILER}")

check_cxx_source_compiles([=[
    #include <expected>

    int main()
    {
        std::expected<int, int> value = 42;
        return value.value();
    }
]=] STDAN_HAS_STD_EXPECTED)

if(NOT STDAN_HAS_STD_EXPECTED)
    message(FATAL_ERROR "std::expected is required; use GCC/libstdc++ or an explicitly configured clang+libc++ toolchain.")
endif()

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

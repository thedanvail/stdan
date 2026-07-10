cmake_minimum_required(VERSION 3.20)
project(stdan LANGUAGES CXX)

option(STDAN_BUILD_TESTS "Build stdan tests" OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(stdan STATIC
    src/memory/arena.cpp
)

target_compile_features(stdan PUBLIC cxx_std_23)
set_target_properties(stdan PROPERTIES CXX_EXTENSIONS OFF)
target_include_directories(stdan PUBLIC include)
target_compile_definitions(stdan PUBLIC
    $<$<CONFIG:Debug>:STDAN_DEBUG>
)

if(STDAN_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

include(FetchContent)

find_package(Catch2 3 QUIET CONFIG)
if(NOT Catch2_FOUND)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.5.4
    )
    FetchContent_MakeAvailable(Catch2)
endif()

add_executable(PopSwapVectorTests EXCLUDE_FROM_ALL PopSwapVectorTests.cpp)
target_link_libraries(PopSwapVectorTests PRIVATE stdan Catch2::Catch2WithMain)
target_compile_definitions(PopSwapVectorTests PRIVATE
    EXPECTED_STDAN_DEBUG=$<IF:$<CONFIG:Debug>,1,0>
)
add_test(NAME PopSwapVectorTests COMMAND PopSwapVectorTests)

add_executable(PopSwapVectorResizeGrowthAbort EXCLUDE_FROM_ALL PopSwapVectorResizeGrowthAbort.cpp)
target_link_libraries(PopSwapVectorResizeGrowthAbort PRIVATE stdan)
add_test(
    NAME PopSwapVectorResizeGrowthAbort
    COMMAND ${CMAKE_COMMAND}
        -DTEST_EXECUTABLE=$<TARGET_FILE:PopSwapVectorResizeGrowthAbort>
        -DSHOULD_ABORT=$<IF:$<CONFIG:Debug>,1,0>
        -P ${CMAKE_CURRENT_SOURCE_DIR}/RunExpectedAbort.cmake
)

add_custom_target(stdan_tests)
add_dependencies(stdan_tests
    PopSwapVectorTests
    PopSwapVectorResizeGrowthAbort
)

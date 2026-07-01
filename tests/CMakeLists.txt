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

add_executable(ps_vector_tests EXCLUDE_FROM_ALL ps_vector_tests.cpp)
target_link_libraries(ps_vector_tests PRIVATE stdan Catch2::Catch2WithMain)
target_compile_definitions(ps_vector_tests PRIVATE
    EXPECTED_STDAN_DEBUG=$<IF:$<CONFIG:Debug>,1,0>
)
add_test(NAME ps_vector_tests COMMAND ps_vector_tests)

add_custom_target(stdan_tests)
add_dependencies(stdan_tests
    ps_vector_tests
)

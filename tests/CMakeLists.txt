file(GLOB TEST_SOURCES CONFIGURE_DEPENDS "*.cpp")

if(STDAN_BUILD_TESTS)
    add_subdirectory(${CMAKE_SOURCE_DIR}/vendor/Catch2 Catch2-build EXCLUDE_FROM_ALL)
else()
    add_library(Catch2::Catch2WithMain INTERFACE IMPORTED GLOBAL)
endif()

add_executable(ps_vector_tests EXCLUDE_FROM_ALL ${TEST_SOURCES})
target_link_libraries(ps_vector_tests PRIVATE stdan Catch2::Catch2WithMain)
target_compile_definitions(ps_vector_tests PRIVATE
    EXPECTED_STDAN_DEBUG=$<IF:$<CONFIG:Debug>,1,0>
)

if(STDAN_BUILD_TESTS)
    add_test(NAME ps_vector_tests COMMAND ps_vector_tests)

    add_custom_target(stdan_tests)
    add_dependencies(stdan_tests
        ps_vector_tests
    )
endif()

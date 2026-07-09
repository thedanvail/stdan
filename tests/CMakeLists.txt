file(GLOB TEST_SOURCES CONFIGURE_DEPENDS "*.cpp")

if(STDAN_BUILD_TESTS)
    if(NOT EXISTS "${CMAKE_SOURCE_DIR}/vendor/Catch2/CMakeLists.txt")
        message(FATAL_ERROR "Catch2 submodule is missing; run git submodule update --init --recursive")
    endif()

    add_subdirectory(${CMAKE_SOURCE_DIR}/vendor/Catch2 Catch2-build EXCLUDE_FROM_ALL)

    add_executable(stdan_tests EXCLUDE_FROM_ALL ${TEST_SOURCES})
    target_link_libraries(stdan_tests PRIVATE stdan Catch2::Catch2WithMain)
    target_compile_definitions(stdan_tests PRIVATE
        EXPECTED_STDAN_DEBUG=$<IF:$<CONFIG:Debug>,1,0>
    )

    add_test(NAME stdan_tests COMMAND stdan_tests)
else()
    add_library(Catch2::Catch2WithMain INTERFACE IMPORTED GLOBAL)
endif()

if(NOT DEFINED TEST_EXECUTABLE)
    message(FATAL_ERROR "TEST_EXECUTABLE must be provided.")
endif()

if(NOT DEFINED SHOULD_ABORT)
    message(FATAL_ERROR "SHOULD_ABORT must be provided.")
endif()

execute_process(
    COMMAND "${TEST_EXECUTABLE}"
    RESULT_VARIABLE TestResult
    OUTPUT_VARIABLE TestStdout
    ERROR_VARIABLE TestStderr
)

if(SHOULD_ABORT)
    if(TestResult STREQUAL "0")
        message(FATAL_ERROR "Expected ${TEST_EXECUTABLE} to abort in this configuration.")
    endif()
    message(STATUS "Observed expected non-zero result: ${TestResult}")
else()
    if(NOT TestResult STREQUAL "0")
        message(FATAL_ERROR
            "Expected ${TEST_EXECUTABLE} to succeed, got ${TestResult}\n"
            "stdout:\n${TestStdout}\n"
            "stderr:\n${TestStderr}"
        )
    endif()
endif()

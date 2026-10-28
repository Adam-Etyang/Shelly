file(MAKE_DIRECTORY "${TEST_BINARY_DIR}")
file(REMOVE
    "${TEST_BINARY_DIR}/simple.txt"
    "${TEST_BINARY_DIR}/pipeline.txt"
    "${TEST_BINARY_DIR}/and.txt"
    "${TEST_BINARY_DIR}/or.txt"
    "${TEST_BINARY_DIR}/result.txt"
    "${TEST_BINARY_DIR}/copied.txt"
    "${TEST_BINARY_DIR}/skipped.txt"
    "${TEST_BINARY_DIR}/stderr.txt"
)

set(input_file "${TEST_BINARY_DIR}/commands.txt")
file(WRITE "${input_file}"
    "echo simple-ok > simple.txt\n"
    "printf pipeline-ok | cat > pipeline.txt\n"
    "true && echo and-ok > and.txt\n"
    "false && echo skipped > skipped.txt\n"
    "false || echo or-ok > or.txt\n"
    "echo redirect-first > result.txt\n"
    "echo redirect-second >> result.txt\n"
    "cat < result.txt > copied.txt\n"
    "sh -c 'printf stderr-ok >&2' 2> stderr.txt\n"
    "exit\n"
)

execute_process(
    COMMAND "${SHELL_EXECUTABLE}"
    WORKING_DIRECTORY "${TEST_BINARY_DIR}"
    INPUT_FILE "${input_file}"
    OUTPUT_VARIABLE shell_stdout
    ERROR_VARIABLE shell_stderr
    RESULT_VARIABLE shell_result
)

if(NOT "${shell_result}" STREQUAL "0")
    message(FATAL_ERROR "Shell exited with ${shell_result}: ${shell_stderr}")
endif()

function(assert_file_content path expected)
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Expected file was not created: ${path}")
    endif()

    file(READ "${path}" actual)
    if(NOT "${actual}" STREQUAL "${expected}")
        message(FATAL_ERROR "Unexpected contents in ${path}: '${actual}'")
    endif()
endfunction()

assert_file_content("${TEST_BINARY_DIR}/simple.txt" "simple-ok\n")
assert_file_content("${TEST_BINARY_DIR}/pipeline.txt" "pipeline-ok")
assert_file_content("${TEST_BINARY_DIR}/and.txt" "and-ok\n")
assert_file_content("${TEST_BINARY_DIR}/or.txt" "or-ok\n")
assert_file_content("${TEST_BINARY_DIR}/result.txt" "redirect-first\nredirect-second\n")
assert_file_content("${TEST_BINARY_DIR}/copied.txt" "redirect-first\nredirect-second\n")
assert_file_content("${TEST_BINARY_DIR}/stderr.txt" "stderr-ok")

if(EXISTS "${TEST_BINARY_DIR}/skipped.txt")
    message(FATAL_ERROR "Right side of a false && expression was executed")
endif()

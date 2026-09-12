#ifndef TEST_SUITES_H
#define TEST_SUITES_H

/*
 * Test suite enum - add new suites here
 *
 * To add a new suite:
 *   1. Add the enum value here
 *   2. Add the run_*() prototype below
 *   3. Create test/test_<suite>.c with the implementation
 *   4. Add the .c file to TEST_MAIN_SRCS in Makefile
 *   5. Add a case in test/test_main.c's switch
 *   6. Add a make shortcut target (test-<suite>)
 */
typedef enum {
  SUITE_ALL,
  SUITE_PARSE,
  SUITE_FORMAT,
  SUITE_CLI,
  SUITE_CAPTURE,
} TestSuite;

void run_parse_tests(void);
void run_format_tests(void);
void run_cli_tests(void);
void run_capture_tests(void);

#endif /* TEST_SUITES_H */

#include "test_framework.h"
#include "test_suites.h"

#include <stdio.h>
#include <string.h>

/*
 * Shared test counters
 */
u32 run    = 0;
u32 passed = 0;

/*
 * Parse suite name from argv
 */
static TestSuite parse_suite(
  const c8 *name
) {
  if (!name || strcmp(name, "all") == 0) return SUITE_ALL;
  if (strcmp(name, "parse") == 0) return SUITE_PARSE;
  if (strcmp(name, "format") == 0) return SUITE_FORMAT;
  if (strcmp(name, "cli") == 0) return SUITE_CLI;
  if (strcmp(name, "capture") == 0) return SUITE_CAPTURE;
  fprintf(stderr, "Unknown test suite: %s\n", name);
  fprintf(stderr, "Available suites: all parse format cli capture\n");
  return SUITE_ALL;
}

/*
 * Main dispatcher
 */
i32 main(
  i32 argc, char **argv
) {
  printf("===== specs_out unit tests =====\n");

  TestSuite suite = (argc > 1) ? parse_suite(argv[1]) : SUITE_ALL;

  switch (suite) {
    case SUITE_PARSE  : run_parse_tests(); break;
    case SUITE_FORMAT : run_format_tests(); break;
    case SUITE_CLI    : run_cli_tests(); break;
    case SUITE_CAPTURE: run_capture_tests(); break;
    case SUITE_ALL:
      run_parse_tests();
      run_format_tests();
      run_cli_tests();
      run_capture_tests();
      break;
  }

  if (run == 0) {
    printf("\n0/0 passed | percentage success: 0.00%%\n");
    return 1;
  }
  printf("\n%u/%u passed | percentage success: %.2f%%\n", passed, run, ((double)passed / (double)run * 100.0));
  return passed == run ? 0 : 1;
}

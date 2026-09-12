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
  const char *name
) {
  if (!name || strcmp(name, "all") == 0) return SUITE_ALL;
  /* -- add new suite names here -- */
  fprintf(stderr, "Unknown test suite: %s\n", name);
  fprintf(stderr, "Available suites: all\n");
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
    case SUITE_ALL:
      /* -- add run_*() calls here as suites are created -- */
      break;
  }

  printf("\n%u/%u passed | percentage success: %.2f%%\n", passed, run, ((double)passed / run * 100.0));
  return passed == run ? 0 : 1;
}

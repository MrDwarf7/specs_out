#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include "../src/prelude.h"

/* Shared test counters - each test file increments these via TEST() */
extern u32 run;
extern u32 passed;

#define TEST_PRINTLN(fmt, ...) do { \
  printf("\n[" fmt "]\n", ##__VA_ARGS__); \
} while (0)

/*
 * Single test - prints PASS/FAIL immediately
 */
#define TEST(name, expr) do { \
  run++; \
  if (expr) { passed++; printf("%3u %-10s|\t%s\n", run, "[PASS]", name); } \
  else      {           printf("%3u %-10s|\t%s\n", run, "**[FAIL]", name); } \
} while (0)

/*
 * Batch test - counts passes/failures in a loop, prints summary at end.
 * Usage:
 *   u32 bp = 0, bf = 0;
 *   for (...) {
 *     BATCH_CHECK("name", expr, bp, bf);
 *   }
 *   BATCH_RESULT("name", bp, bf);
 */
#define BATCH_CHECK(name, expr, bp, bf) do { \
  run++; \
  if (expr) { passed++; bp++; } \
  else      { bf++; printf("  **[FAIL] %s (iteration %u)\n", name, bp + bf); } \
} while (0)

#define BATCH_RESULT(name, bp, bf) do { \
  if (bf == 0) { \
    printf("%3u %-10s|\t%s (%u iterations)\n", run, "[PASS]", name, bp); \
  } else { \
    printf("%3u %-10s|\t%s - %u/%u passed\n", run, "**[FAIL]", name, bp, bp + bf); \
  } \
} while (0)

/* Typed assertion macros -- wrap TEST() with descriptive failure messages. */
#define TEST_ASSERT(name, expr)            TEST(name, (expr))
#define TEST_ASSERT_TRUE(name, expr)       TEST(name, !!(expr))
#define TEST_ASSERT_FALSE(name, expr)      TEST(name, !(expr))
#define TEST_ASSERT_EQ(name, a, b)         TEST(name, (a) == (b))
#define TEST_ASSERT_NEQ(name, a, b)        TEST(name, (a) != (b))
#define TEST_ASSERT_NULL(name, ptr)        TEST(name, (ptr) == nullptr)
#define TEST_ASSERT_NOT_NULL(name, ptr)    TEST(name, (ptr) != nullptr)
#define TEST_ASSERT_ZERO(name, val)        TEST(name, (val) == 0)
#define TEST_ASSERT_GT(name, a, b)         TEST(name, (a) > (b))
#define TEST_ASSERT_GTE(name, a, b)        TEST(name, (a) >= (b))
#define TEST_ASSERT_LT(name, a, b)         TEST(name, (a) < (b))
#define TEST_ASSERT_LTE(name, a, b)        TEST(name, (a) <= (b))

/*
 * TEST_RUN -- run a named test function and track pass/fail.
 * Usage: TEST_RUN("suite setup", run_my_setup);
 */
#define TEST_RUN(name, func) do { \
  run++; \
  bool _ok = (func); \
  if (_ok) { passed++; printf("%3u %-10s|\t%s\n", run, "[PASS]", name); } \
  else     {           printf("%3u %-10s|\t%s\n", run, "**[FAIL]", name); } \
} while (0)

/*
 * TEST_SUMMARY -- print final pass/fail counts.
 * Call at the end of each suite's run function.
 */
#define TEST_SUMMARY(suite_name) do { \
  u32 _fail = run - passed; \
  if (_fail == 0) { \
    printf("\n%s: %u/%u passed\n", (suite_name), passed, run); \
  } else { \
    printf("\n%s: %u/%u passed (%u FAILED)\n", (suite_name), passed, run, _fail); \
  } \
} while (0)

#endif /* TEST_FRAMEWORK_H */

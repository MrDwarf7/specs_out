#define _GNU_SOURCE

#include "specs.h"
#include "test_framework.h"
#include "test_suites.h"

#include <stdio.h>
#include <string.h>

static void expect_str(
  const c8 *name, const c8 *got, const c8 *expected
) {
  if (strcmp(got ? got : "", expected ? expected : "") != 0) { printf("  got: [%s]\n  exp: [%s]\n", got ? got : "(null)", expected ? expected : "(null)"); }
  TEST(name, got && expected && strcmp(got, expected) == 0);
}

void run_cli_tests(
  void
) {
  TEST_PRINTLN("cli");

  SpecsCli cli            = {0};
  c8      *argv_default[] = {"specs_out", nullptr};
  TEST("no destination selects stdout", specs_parse_args(1, argv_default, &cli) && cli.dest == nullptr);

  c8 *argv_help[] = {"specs_out", "--help", nullptr};
  TEST("parse --help", specs_parse_args(2, argv_help, &cli) && cli.help);

  c8 *argv_h[] = {"specs_out", "-h", nullptr};
  TEST("parse -h", specs_parse_args(2, argv_h, &cli) && cli.help);

  c8 *argv_dry[] = {"specs_out", "--dry-run", "-o", "/tmp/out", nullptr};
  TEST("parse --dry-run -o", specs_parse_args(4, argv_dry, &cli) && cli.dry_run);
  expect_str("parse -o dest", cli.dest, "/tmp/out");

  c8 *argv_output[] = {"specs_out", "--output", "/tmp/report", nullptr};
  TEST("parse --output", specs_parse_args(3, argv_output, &cli));
  expect_str("long output dest", cli.dest, "/tmp/report");

  c8 *argv_n[] = {"specs_out", "-n", nullptr};
  TEST("parse -n", specs_parse_args(2, argv_n, &cli) && cli.dry_run);

  c8 *argv_t[] = {"specs_out", "-t", nullptr};
  TEST("parse -t", specs_parse_args(2, argv_t, &cli) && cli.force && cli.dest == nullptr);

  c8 *argv_y[] = {"specs_out", "--yes", nullptr};
  TEST("parse --yes", specs_parse_args(2, argv_y, &cli) && cli.force);

  c8 *argv_trunc[] = {"specs_out", "--truncate", nullptr};
  TEST("parse --truncate", specs_parse_args(2, argv_trunc, &cli) && cli.force);

  c8 *argv_pos[] = {"specs_out", "/alt/path", nullptr};
  TEST("parse positional", specs_parse_args(2, argv_pos, &cli));
  expect_str("positional dest", cli.dest, "/alt/path");

  c8 *argv_dd[] = {"specs_out", "--", "/after", nullptr};
  TEST("parse -- dest", specs_parse_args(3, argv_dd, &cli));
  expect_str("-- dest", cli.dest, "/after");

  c8 *argv_bad[] = {"specs_out", "--nope", nullptr};
  TEST("parse unknown", !specs_parse_args(2, argv_bad, &cli) && cli.bad_flag);
  expect_str("unknown flag name", cli.unknown, "--nope");

  c8 *argv_miss[] = {"specs_out", "-o", nullptr};
  TEST("parse missing -o arg", !specs_parse_args(2, argv_miss, &cli) && cli.missing_arg);

  TEST("confirm write new", specs_confirm_ex(false, false, false) == SPECS_WRITE);
  TEST("confirm write force", specs_confirm_ex(true, true, false) == SPECS_WRITE);
  TEST("confirm refuse non-tty", specs_confirm_ex(true, false, false) == SPECS_REFUSE);
  TEST("confirm prompt tty", specs_confirm_ex(true, false, true) == SPECS_PROMPT);

  TEST("answer y", specs_answer_is_yes("y\n"));
  TEST("answer Y", specs_answer_is_yes("Y"));
  TEST("answer yes", specs_answer_is_yes("yes"));
  TEST("answer YES", specs_answer_is_yes("YES"));
  TEST("answer no", !specs_answer_is_yes("n"));
  TEST("answer empty", !specs_answer_is_yes(""));

  TEST_SUMMARY("cli");
}

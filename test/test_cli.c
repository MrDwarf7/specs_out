#define _GNU_SOURCE

#include "specs.h"
#include "test_framework.h"
#include "test_suites.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

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

  u8    mem[4096];
  Arena arena;
  arena_init(&arena, mem, sizeof(mem));

  c8 *dest = nullptr;
  TEST("default dest ok", specs_default_dest(&arena, "/home/me", &dest));
  expect_str("default dest path", dest, "/home/me/.specs");
  TEST("default dest missing home", !specs_default_dest(&arena, nullptr, &dest));
  TEST("default dest empty home", !specs_default_dest(&arena, "", &dest));

  c8      *argv_help[] = {"specs_out", "--help", nullptr};
  SpecsCli cli         = {0};
  TEST("parse --help", specs_parse_args(2, argv_help, "/home/me/.specs", &cli) && cli.help);

  c8 *argv_h[] = {"specs_out", "-h", nullptr};
  TEST("parse -h", specs_parse_args(2, argv_h, "/home/me/.specs", &cli) && cli.help);

  c8 *argv_dry[] = {"specs_out", "--dry-run", "-o", "/tmp/out", nullptr};
  TEST("parse --dry-run -o", specs_parse_args(4, argv_dry, "/home/me/.specs", &cli) && cli.dry_run);
  expect_str("parse -o dest", cli.dest, "/tmp/out");

  c8 *argv_n[] = {"specs_out", "-n", nullptr};
  TEST("parse -n", specs_parse_args(2, argv_n, "/home/me/.specs", &cli) && cli.dry_run);

  c8 *argv_t[] = {"specs_out", "-t", nullptr};
  TEST("parse -t", specs_parse_args(2, argv_t, "/home/me/.specs", &cli) && cli.force);

  c8 *argv_y[] = {"specs_out", "--yes", nullptr};
  TEST("parse --yes", specs_parse_args(2, argv_y, "/home/me/.specs", &cli) && cli.force);

  c8 *argv_trunc[] = {"specs_out", "--truncate", nullptr};
  TEST("parse --truncate", specs_parse_args(2, argv_trunc, "/home/me/.specs", &cli) && cli.force);

  c8 *argv_pos[] = {"specs_out", "/alt/path", nullptr};
  TEST("parse positional", specs_parse_args(2, argv_pos, "/home/me/.specs", &cli));
  expect_str("positional dest", cli.dest, "/alt/path");

  c8 *argv_dd[] = {"specs_out", "--", "/after", nullptr};
  TEST("parse -- dest", specs_parse_args(3, argv_dd, "/home/me/.specs", &cli));
  expect_str("-- dest", cli.dest, "/after");

  c8 *argv_bad[] = {"specs_out", "--nope", nullptr};
  TEST("parse unknown", !specs_parse_args(2, argv_bad, "/home/me/.specs", &cli) && cli.bad_flag);
  expect_str("unknown flag name", cli.unknown, "--nope");

  c8 *argv_miss[] = {"specs_out", "-o", nullptr};
  TEST("parse missing -o arg", !specs_parse_args(2, argv_miss, "/home/me/.specs", &cli) && cli.missing_arg);

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

  c8    help_buf[2048];
  FILE *help_out = fmemopen(help_buf, sizeof(help_buf), "w");
  TEST("usage fmemopen", help_out != nullptr);
  if (help_out) {
    specs_usage("specs_out", "/home/me/.specs", help_out);
    fclose(help_out);
    TEST("usage Usage:", strstr(help_buf, "Usage:") != nullptr);
    TEST("usage -t/--truncate", strstr(help_buf, "-t, --truncate") != nullptr);
    TEST("usage -y/--yes", strstr(help_buf, "-y, --yes") != nullptr);
    TEST("usage -n/--dry-run", strstr(help_buf, "-n, --dry-run") != nullptr);
    TEST("usage -o/--output", strstr(help_buf, "-o, --output PATH") != nullptr);
    TEST("usage -h/--help", strstr(help_buf, "-h, --help") != nullptr);
    TEST("usage positional PATH", strstr(help_buf, "PATH as a positional") != nullptr);
    TEST("usage default dest", strstr(help_buf, "/home/me/.specs") != nullptr);
  }

  (void)isatty;
  TEST_SUMMARY("cli");
}

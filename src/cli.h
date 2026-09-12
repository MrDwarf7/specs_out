#ifndef SPECS_CLI_H
#define SPECS_CLI_H

#include "prelude.h"

#include <stdalign.h>
#include <stdio.h>

/*
 * ## Summary
 * Parsed CLI flags. `dest` points at argv, or is null for stdout.
 *
 */
typedef struct {
  const c8 *dest;
  const c8 *unknown;
  const c8 *opt_name;
  bool      force;
  bool      dry_run;
  bool      help;
  bool      bad_flag;
  bool      missing_arg;
} ATTR_PACKED_ALIGNED(8) SpecsCli;

_Static_assert(sizeof(SpecsCli) == 32, "SpecsCli: pointers first, flags in the tail pad");

/*
 * ## Summary
 * Result of dest-exists / force / TTY checks (prompt itself is separate).
 *
 */
typedef enum {
  SPECS_WRITE = 0,
  SPECS_SKIP,
  SPECS_REFUSE,
  SPECS_PROMPT,
} SpecsConfirm;

/*
 * ## Summary
 * Parse argv into `out`. Does not exit. No destination selects stdout.
 *
 * ## Returns
 * `true` when args are usable (including `--help`),
 * `false` on unknown flag or missing `-o` argument.
 *
 */
bool specs_parse_args(i32 argc, c8 **argv, SpecsCli *out);

/*
 * ## Summary
 * Decide write vs refuse vs TTY prompt from dest existence, force, and isatty.
 *
 * ## Returns
 * `SPECS_WRITE` when dest is absent or force is set,
 * `SPECS_REFUSE` when dest exists, not a TTY, and force is unset,
 * `SPECS_PROMPT` when dest exists, stdin is a TTY, and force is unset.
 *
 */
SpecsConfirm specs_confirm_ex(bool dest_exists, bool force, bool stdin_is_tty);

/*
 * ## Summary
 * True for `y` / `Y` / `yes` / `YES` after trim.
 *
 * ## Returns
 * `true` if the answer is yes,
 * `false` otherwise.
 *
 */
bool specs_answer_is_yes(const c8 *answer);

/*
 * ## Summary
 * Write usage text to `out`.
 *
 */
void specs_usage(const c8 *program, FILE *out);

#endif /* SPECS_CLI_H */

#define _GNU_SOURCE

#include "arena.h"
#include "cli.h"
#include "gather.h"
#include "prelude.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static bool prompt_yes(
  const c8 *dest
) {
  fprintf(stderr, "File exists: %s\nReplace with new version? [y/N] ", dest);
  fflush(stderr);
  c8 answer[32] = "";
  if (!fgets(answer, (i32)sizeof(answer), stdin)) return false;
  return specs_answer_is_yes(answer);
}

/*
 * ## Summary
 * Print the buffered report and detect write or flush failures.
 *
 * ## Returns
 * Returns after flushing stdout, or exits on an output error.
 *
 */
static void print_document(
  const c8 *doc
) {
  /* The complete report is already buffered in the document arena. */
  usize doc_len = strlen(doc);
  if (fwrite(doc, 1, doc_len, stdout) != doc_len || fflush(stdout) != 0) { specs_die("could not write stdout: %s", strerror(errno)); }
}

i32 main(
  i32 argc, c8 **argv
) {
  specs_runtime_init();
  Arena *arena = specs_doc_arena();

  SpecsCli cli = {};
  if (!specs_parse_args(argc, argv, &cli)) {
    if (cli.missing_arg) specs_die("Option %s requires an argument", cli.opt_name);
    fprintf(stderr, ERR_PREFIX "Unknown flag: %s\n", cli.unknown ? cli.unknown : "");
    specs_usage(argv[0], stdout);
    specs_runtime_fini();
    return EXIT_FAILURE;
  }
  if (cli.help) {
    specs_usage(argv[0], stdout);
    specs_runtime_fini();
    return 0;
  }

  const c8 *dest = cli.dest;
  const c8 *doc  = specs_gather_document(arena);

  if (!dest || cli.dry_run) {
    if (cli.dry_run && dest) fprintf(stderr, "[DRY] would write to %s\n", dest);
    print_document(doc);
    specs_runtime_fini();
    return 0;
  }

  bool         exists  = access(dest, F_OK) == 0;
  bool         is_tty  = isatty(STDIN_FILENO) == 1;
  SpecsConfirm confirm = specs_confirm_ex(exists, cli.force, is_tty);
  if (confirm == SPECS_REFUSE) specs_die("%s exists; pass -t or -y to replace", dest);
  if (confirm == SPECS_PROMPT && !prompt_yes(dest)) {
    fprintf(stderr, "Not writing %s\n", dest);
    print_document(doc);
    specs_runtime_fini();
    return 0;
  }

  FILE *out = fopen(dest, "w");
  if (!out) specs_die("cannot open %s: %s", dest, strerror(errno));
  usize doc_len = strlen(doc);
  if (fwrite(doc, 1, doc_len, out) != doc_len || fclose(out) != 0) { specs_die("could not write %s: %s", dest, strerror(errno)); }
  fprintf(stderr, "Wrote %s\n", dest);
  specs_runtime_fini();
  return 0;
}

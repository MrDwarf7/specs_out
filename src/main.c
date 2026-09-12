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

i32 main(
  i32 argc, c8 **argv
) {
  specs_runtime_init();
  Arena *arena           = specs_doc_arena();

  c8       *default_dest = nullptr;
  const c8 *home         = getenv("HOME");
  if (!specs_default_dest(arena, home, &default_dest)) {
    bool needs_default = true;
    for (i32 idx = 1; idx < argc; idx++) {
      if (strcmp(argv[idx], "-h") == 0 || strcmp(argv[idx], "--help") == 0) { specs_die("HOME is not set; pass -o PATH"); }
      if (strcmp(argv[idx], "-o") == 0 || strcmp(argv[idx], "--output") == 0) needs_default = false;
      if (argv[idx][0] != '-') needs_default = false;
    }
    if (needs_default) specs_die("HOME is not set; pass -o PATH");
    default_dest = arena_copy_str(arena, "");
  }

  SpecsCli cli = {};
  if (!specs_parse_args(argc, argv, default_dest, &cli)) {
    if (cli.missing_arg) specs_die("Option %s requires an argument", cli.opt_name);
    fprintf(stderr, ERR_PREFIX "Unknown flag: %s\n", cli.unknown ? cli.unknown : "");
    specs_usage(argv[0], default_dest, stdout);
    specs_runtime_fini();
    return EXIT_FAILURE;
  }
  if (cli.help) {
    specs_usage(argv[0], default_dest, stdout);
    specs_runtime_fini();
    return 0;
  }

  const c8 *dest = cli.dest ? cli.dest : default_dest;
  const c8 *doc  = specs_gather_document(arena);

  if (cli.dry_run) {
    fprintf(stderr, "[DRY] would write to %s\n", dest);
    fputs(doc, stdout);
    specs_runtime_fini();
    return 0;
  }

  bool         exists  = access(dest, F_OK) == 0;
  bool         is_tty  = isatty(STDIN_FILENO) == 1;
  SpecsConfirm confirm = specs_confirm_ex(exists, cli.force, is_tty);
  if (confirm == SPECS_REFUSE) specs_die("%s exists; pass -t or -y to replace", dest);
  if (confirm == SPECS_PROMPT && !prompt_yes(dest)) {
    fprintf(stderr, "Not writing %s\n", dest);
    fputs(doc, stdout);
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

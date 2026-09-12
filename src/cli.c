#define _GNU_SOURCE

#include "cli.h"

#include <string.h>

bool specs_parse_args(
  i32 argc, c8 **argv, SpecsCli *out
) {
  if (!out) return false;
  ZERO_MEMORY(out, sizeof(*out));
  if (argc < 1 || !argv) return true;

  for (i32 idx = 1; idx < argc; idx++) {
    const c8 *arg = argv[idx];
    if (!arg) continue;
    if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
      out->help = true;
      return true;
    }
    if (strcmp(arg, "-t") == 0 || strcmp(arg, "--truncate") == 0 || strcmp(arg, "-y") == 0 || strcmp(arg, "--yes") == 0) {
      out->force = true;
      continue;
    }
    if (strcmp(arg, "-n") == 0 || strcmp(arg, "--dry-run") == 0) {
      out->dry_run = true;
      continue;
    }
    if (strcmp(arg, "-o") == 0 || strcmp(arg, "--output") == 0) {
      if (idx + 1 >= argc) {
        out->missing_arg = true;
        out->opt_name    = arg;
        return false;
      }
      idx++;
      out->dest = argv[idx];
      continue;
    }
    if (strcmp(arg, "--") == 0) {
      if (idx + 1 < argc) out->dest = argv[idx + 1];
      break;
    }
    if (arg[0] == '-') {
      out->bad_flag = true;
      out->unknown  = arg;
      return false;
    }
    out->dest = arg;
  }
  return true;
}

SpecsConfirm specs_confirm_ex(
  bool dest_exists, bool force, bool stdin_is_tty
) {
  if (!dest_exists || force) return SPECS_WRITE;
  if (!stdin_is_tty) return SPECS_REFUSE;
  return SPECS_PROMPT;
}

bool specs_answer_is_yes(
  const c8 *answer
) {
  if (!answer) return false;
  const c8 *start = answer;
  while (*start && (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r'))
    start++;
  usize len = strlen(start);
  while (len && (start[len - 1] == ' ' || start[len - 1] == '\t' || start[len - 1] == '\n' || start[len - 1] == '\r'))
    len--;
  if (len == 1) return ascii_to_lower((unsigned char)start[0]) == (unsigned char)'y';
  if (len == 3) {
    return ascii_eq_ci(
      (const c8[]){
        (c8)ascii_to_lower((unsigned char)start[0]),
        (c8)ascii_to_lower((unsigned char)start[1]),
        (c8)ascii_to_lower((unsigned char)start[2]),
        '\0',
      },
      "yes"
    );
  }
  return false;
}

void specs_usage(
  const c8 *program, FILE *out
) {
  const c8 *base = nullptr;
  if (program) {
    const c8 *slash = strrchr(program, '/');
    base            = slash ? slash + 1 : program;
  } else {
    base = "specs_out";
  }
  fprintf(
    out ? out : stdout,
    "Usage: %s [options] [PATH]\n\n"
    "Gather hardware/OS facts from system commands and print a document\n"
    "to stdout. No files are written unless PATH is supplied.\n\n"
    "Options:\n"
    "  -t, --truncate      Replace an existing file without asking\n"
    "  -y, --yes           Same as --truncate (assume yes)\n"
    "  -n, --dry-run       Print the document, write nothing\n"
    "  -o, --output PATH   Write to PATH instead of stdout\n"
    "  -h, --help          Show this help\n\n"
    "PATH as a positional argument is the same as -o PATH.\n\n"
    "If the destination already exists and neither -t nor -y was passed,\n"
    "you are asked before replacing it. Non-interactive stdin refuses to\n"
    "clobber without -t/-y.\n",
    base
  );
}

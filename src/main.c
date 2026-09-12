#define _GNU_SOURCE

#include "prelude.h"

#include <stdio.h>
#include <stdlib.h>

// Or
// i32 main(int argc, char *argv[]) {
i32 main(
  i32 argc, char **argv
) {
  (void)argc;
  (void)argv;
  //
  printf("Hello, World!\n");

  return EXIT_SUCCESS;
}

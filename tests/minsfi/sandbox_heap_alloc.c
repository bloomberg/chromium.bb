/*
 * Copyright (c) 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, const char **argv) {
  if (argc == 2) {
    long func_arg = strtol(argv[1], NULL, 0);

    /* Run sbrk(), return the new program break or (-1) if sbrk() failed. */
    if (!strcmp(argv[0], "sbrk")) {
      if (sbrk(func_arg) == (void*) -1)
        return -1;
      return (int) sbrk(0);  /* get the new program break */
    }

    /* Allocate the given amount of memory with malloc. */
    if (!strcmp(argv[0], "malloc"))
      return (int) malloc(func_arg);
  }

  return EXIT_FAILURE;
}

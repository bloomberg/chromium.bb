/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


int main(int argc, char **argv, char **env) {
  int count = 0;
  char **ptr;

  for (ptr = environ; *ptr != NULL; ptr++) {
    count++;
  }
  printf("%i environment variables\n", count);
  for (ptr = environ; *ptr != NULL; ptr++) {
    printf("%s\n", *ptr);
  }

  assert(env == environ);

  return 0;
}

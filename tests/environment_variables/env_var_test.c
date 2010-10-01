/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


int main(int argc, char **argv, char **env) {
  int count = 0;
  char **ptr;
  char *value;
  int compare;

  for (ptr = environ; *ptr != NULL; ptr++) {
    count++;
  }
  printf("%i environment variables\n", count);
  for (ptr = environ; *ptr != NULL; ptr++) {
    printf("%s\n", *ptr);
  }

  assert(env == environ);

  value = getenv("NACL_SRPC_DEBUG");
  assert(value != NULL);
  /* TODO(mseaborn): Splitting this into two lines is a workaround for
     http://code.google.com/p/nativeclient/issues/detail?id=941.
     -Werror -pedantic -O2 + glibc conspire to produce fail. */
  compare = strcmp(value, "example_contents_of_env_var");
  assert(compare == 0);

  return 0;
}

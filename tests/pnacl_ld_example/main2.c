/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdio.h>
#include "simple.h"

int main(int argc, char** argv) {
  printf("before shlib calls\n");
  fflush(stdout);

  printf("hello returned '%s'\n", hello());
  printf("fortytwo returned: %d\n", fortytwo());

  printf("hello2 returned '%s'\n", hello2());
  printf("fortytwo2 returned '%d'\n", fortytwo2());
  printf("after shlib calls\n");
  fflush(stdout);
  return 0;
}

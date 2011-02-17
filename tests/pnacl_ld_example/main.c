/* Copyright 2011 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <stdio.h>
#include "simple.h"

int main(int argc, char** argv) {
  printf("hello returned '%s'\n", hello());
  printf("fortytwo returned: %d\n", fortytwo());
  return 0;
}

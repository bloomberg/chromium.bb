/*
 * Copyright 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Hello world with floating point in the mix.
 */

#include <stdio.h>

int main(int argc, char* argv[]) {
  /* The constant 2.0 causes the compiler to generate floating point
   * instructions. Compiler flags are used to control what kind of
   * instructions (x87, SSE etc.) are generated.
   */
  if (argc < 2.0) printf("Hello, World!\n");
  return 0;
}

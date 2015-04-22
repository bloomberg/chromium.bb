/*
 * Copyright (c) 2015 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdio.h>

/*
 * A simple program that makes it simple to test that all the arguments
 * were passed correctly.
 */

int main(int argc, char **argv) {
  for (int i = 1; i < argc; ++i) {
    puts(argv[i]);
  }
  return 0;
}

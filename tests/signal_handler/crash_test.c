/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <stdio.h>

int main() {
  fprintf(stderr, "Causing crash in untrusted code...\n");
  *(int *) 0 = 0;
  fprintf(stderr, "FAIL: Survived crash attempt\n");
  return 1;
}

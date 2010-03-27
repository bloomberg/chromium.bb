/*
 * Copyright 2010 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */
#include <stdio.h>

#include "native_client/src/trusted/nacl_breakpad/nacl_breakpad.h"

int main() {
  NaClBreakpadInit();

  volatile char* pChar = 0;
  *pChar = 'A';
  printf("FAIL: continued after null dereference");

  NaClBreakpadFini();

  return 0;
}

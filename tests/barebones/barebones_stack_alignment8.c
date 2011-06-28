/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "barebones.h"


int main() {
  NACL_SYSCALL(exit)(55 + ((int)__builtin_dwarf_cfa() & 7));
  /* UNREACHABLE */
  return 0;
}

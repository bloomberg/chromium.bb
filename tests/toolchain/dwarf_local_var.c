/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Test that dwarf info for local variables is preserved after linking.
 *
 * We have checks encoded as "CHECK: regex", and the regexes must be
 * matched in-order in the disassembled file.  An external tool
 * "file-check" will do the checks provided this source file and the binary.
 */

#include <stdlib.h>

extern int bar(int x) __attribute__((noinline));

__attribute__((noinline)) int foo(int dwarf_test_param) {
  /* CHECK: DW_TAG_formal_parameter */
  /* CHECK: DW_AT_name.* dwarf_test_param */
  /* CHECK: DW_AT_decl_line.* 18 */
  return bar(dwarf_test_param);
}

int main(int argc, char* argv[]) {
  int dwarf_test_local;
  /* CHECK: DW_TAG_variable */
  /* CHECK: DW_AT_name.* dwarf_test_local */
  /* CHECK: DW_AT_decl_line.* 26 */

  /* Try to trick the optimizer to preserve dwarf_test_local. */
  if (argc != 55) {
    /* This branch should be taken. */
    dwarf_test_local = 55;
  } else if (argc == 54) {
    dwarf_test_local = atoi(argv[1]);
  } else {
    dwarf_test_local = -1;
  }

  if (argc != 54) {
    /* This branch should be taken. */
    return foo(dwarf_test_local);
  } else {
    return -1;
  }
}

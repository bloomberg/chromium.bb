/*
 * Copyright (c) 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/tests/irt_ext/error_report.h"
#include "native_client/tests/irt_ext/file_desc.h"
#include "native_client/tests/irt_ext/mem_calls.h"
#include "native_client/tests/irt_ext/libc/libc_test.h"

#include <stdio.h>

int main(void) {
  int ret = 0;

  /* Initialize the various modules. */
  init_error_report_module();
  init_file_desc_module();
  init_mem_calls_module();

  /* Run tests. */
  ret += run_file_tests();
  ret += run_mem_tests();

  return ret;
}

/*
 * Copyright (c) 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/tests/pnacl_dynamic_loading/test_pso.h"


static int var = 2345;

/* Zero-initialized variables go in the BSS.  Test a large BSS. */
static char bss_var[BSS_VAR_SIZE];

static int example_func(int *ptr) {
  return *ptr + 1234;
}

static int *get_var(void) {
  /* Test use of -fPIC by getting an address. */
  return &var;
}

struct test_pso_root __pnacl_pso_root = {
  example_func,
  get_var,
  bss_var,
};

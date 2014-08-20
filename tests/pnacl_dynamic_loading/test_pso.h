/*
 * Copyright (c) 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_TESTS_PNACL_DYNAMIC_LOADING_TEST_PSO_H_
#define NATIVE_CLIENT_TESTS_PNACL_DYNAMIC_LOADING_TEST_PSO_H_ 1

#define BSS_VAR_SIZE 0x20000

struct test_pso_root {
  int (*example_func)(int *ptr);
  int *(*get_var)(void);
  char *bss_var;
};

#endif

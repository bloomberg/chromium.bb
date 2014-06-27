/*
 * Copyright (c) 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

static int var = 2345;

static int example_func(int *ptr) {
  return *ptr + 1234;
}

static int *get_var(void) {
  /* Test use of -fPIC by getting an address. */
  return &var;
}

struct pso_root {
  int (*example_func)(int *ptr);
  int *(*get_var)(void);
};

struct pso_root __pnacl_pso_root = {
  example_func,
  get_var,
};

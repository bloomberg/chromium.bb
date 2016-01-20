/*
 * Copyright (c) 2015 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* This is an example library, for testing the ConvertToPSO pass. */

int var = 2345;

int *get_var(void) {
  return &var;
}

int example_func(void) {
  return 5678;
}

/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

int global_var;

void set_global_var(int arg) {
  global_var = arg;
}

int main() {
  global_var = 2;
  set_global_var(1);
  return global_var;
}

/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Sanity test for AddressSanitizer. */

int main(void) {
  volatile char a[1] = {0};
  volatile int idx = 1;
  return a[idx];
}

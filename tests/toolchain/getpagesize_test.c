/*
 * Copyright (c) 2013 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <assert.h>
#include <unistd.h>

int main(void) {
  /* The page size is always 64k under NaCl. */
  assert(getpagesize() == 0x10000);
  return 0;
}

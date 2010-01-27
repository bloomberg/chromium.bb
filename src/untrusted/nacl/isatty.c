/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Stub routine for isatty for newlib support.
 */

#include <unistd.h>

int isatty(int file) {
  /* NOT IMPLEMENTED: isatty */
  /* isatty does not have an error return, so we simply say yes always. */
  return 1;
}

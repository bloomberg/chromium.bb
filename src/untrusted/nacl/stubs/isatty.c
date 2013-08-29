/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Stub routine for `isatty' for porting support.
 */

#include <unistd.h>
#include <errno.h>

int isatty(int file) {
  errno = ENOSYS;
  /* isatty does not have an error return, so we simply say `no' always. */
  return 0;
}

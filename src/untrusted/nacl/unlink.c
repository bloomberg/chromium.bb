/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


/*
 * Stub routine for unlink for newlib support.
 */

#include <unistd.h>
#include <errno.h>

int unlink(const char *path) {
  /* NOTE(sehr): NOT IMPLEMENTED: unlink */
  errno = ENOENT;
  return -1;
}

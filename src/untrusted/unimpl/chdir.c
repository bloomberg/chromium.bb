/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


/*
 * Stub routines for chdir/fchdir for porting support.
 */

#include <unistd.h>
#include <errno.h>

int chdir(const char *path) {
  /* NOTE(sehr): NOT IMPLEMENTED: chdir */
  errno = EACCES;
  return -1;
}

int fchdir(int fd) {
  /* NOTE(sehr): NOT IMPLEMENTED: fchdir */
  errno = EACCES;
  return -1;
}

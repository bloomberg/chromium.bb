/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


/*
 * Stub routines for chmod/fchmod for porting support.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

int chmod(const char *path, mode_t mode) {
  /* NOTE(sehr): NOT IMPLEMENTED: chmod */
  errno = EACCES;
  return -1;
}

int fchmod(int filedes, mode_t mode) {
  /* NOTE(sehr): NOT IMPLEMENTED: fchmod */
  errno = EACCES;
  return -1;
}

/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


/*
 * Stub routines for chown/fchown/lchown for porting support.
 */

#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

int chown(const char *path, uid_t owner, gid_t group) {
  /* NOTE(sehr): NOT IMPLEMENTED: chown */
  errno = EACCES;
  return -1;
}

int fchown(int fd, uid_t owner, gid_t group) {
  /* NOTE(sehr): NOT IMPLEMENTED: fchown */
  errno = EACCES;
  return -1;
}

int lchown(const char *path, uid_t owner, gid_t group) {
  /* NOTE(sehr): NOT IMPLEMENTED: fchown */
  errno = EACCES;
  return -1;
}

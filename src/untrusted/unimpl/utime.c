/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


/*
 * Stub routines for utime/utimes for porting support.
 */

#include <sys/types.h>
#include <utime.h>
#include <sys/time.h>
#include <errno.h>

int utime(const char *filename, const struct utimbuf *buf) {
  /* NOTE(sehr): NOT IMPLEMENTED: utime */
  errno = EACCES;
  return -1;
}

int utimes(const char *filename, const struct timeval tv[2]) {
  /* NOTE(sehr): NOT IMPLEMENTED: utimes */
  errno = EACCES;
  return -1;
}

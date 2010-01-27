/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


/*
 * Stub routines for getuid/geteuid for porting support.
 */

#include <unistd.h>
#include <sys/types.h>
#include <errno.h>

uid_t getuid(void) {
  /* NOTE(sehr): NOT IMPLEMENTED: getuid */
  errno = ENOSYS;
  return -1;
}

uid_t geteuid(void) {
  /* NOTE(sehr): NOT IMPLEMENTED: geteuid */
  errno = ENOSYS;
  return -1;
}

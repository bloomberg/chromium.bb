/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


/*
 * Stub routines for getgid/getegid for porting support.
 */

#include <unistd.h>
#include <sys/types.h>
#include <errno.h>

gid_t getgid(void) {
  /* NOTE(sehr): NOT IMPLEMENTED: getgid */
  errno = ENOSYS;
  return -1;
}

gid_t getegid(void) {
  /* NOTE(sehr): NOT IMPLEMENTED: getegid */
  errno = ENOSYS;
  return -1;
}

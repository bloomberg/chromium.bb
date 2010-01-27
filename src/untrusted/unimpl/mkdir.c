/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


/*
 * Stub routine for mkdir for porting support.
 */

#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

int mkdir(const char *pathname, mode_t mode) {
  /* NOTE(sehr): NOT IMPLEMENTED: mkdir */
  errno = EACCES;
  return -1;
}

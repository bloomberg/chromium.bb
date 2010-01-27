/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


/*
 * Stub routines for fcntl for porting support.
 */

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

int fcntl(int fd, int cmd, ...) {
  /* NOTE(sehr): NOT IMPLEMENTED: fcntl */
  errno = EACCES;
  return -1;
}

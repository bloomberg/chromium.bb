/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


/*
 * Stub routines for ioctl for porting support.
 */

/* #include <ioctl.h> */
#include <errno.h>

int ioctl(int d, int request) {
  /* NOTE(sehr): NOT IMPLEMENTED: ioctl */
  errno = EBADF;
  return -1;
}

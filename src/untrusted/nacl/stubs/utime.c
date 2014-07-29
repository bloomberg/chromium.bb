/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


/*
 * Stub routine for `utime' for porting support.
 */

#include <sys/types.h>
#include <utime.h>
#include <sys/time.h>
#include <errno.h>

int utime(const char *filename, const struct utimbuf *buf) {
  errno = ENOSYS;
  return -1;
}

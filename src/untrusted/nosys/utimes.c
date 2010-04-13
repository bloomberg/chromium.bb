/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


/*
 * Stub routine for `utimes' for porting support.
 */

#include <sys/types.h>
#include <utime.h>
#include <sys/time.h>
#include <errno.h>

int utimes(const char *filename, const struct timeval tv[2]) {
  errno = ENOSYS;
  return -1;
}

#include "native_client/src/untrusted/nosys/warning.h"
stub_warning(utimes);

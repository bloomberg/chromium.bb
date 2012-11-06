/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


/*
 * Stub routine for `chmod' for porting support.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

int chmod(const char *path, mode_t mode) {
  errno = ENOSYS;
  return -1;
}

#include "native_client/src/untrusted/nosys/warning.h"
stub_warning(chmod);

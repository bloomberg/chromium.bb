/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


/*
 * Stub routine for `readlink' for porting support.
 */

#include <unistd.h>
#include <errno.h>

int readlink(const char *path, char *buf, int bufsiz) {
  errno = ENOSYS;
  return -1;
}

#include "native_client/src/untrusted/nosys/warning.h"
stub_warning(readlink);

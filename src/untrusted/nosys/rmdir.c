/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


/*
 * Stub routine for `rmdir' for porting support.
 */

#include <unistd.h>
#include <errno.h>

int rmdir(const char *pathname) {
  errno = ENOSYS;
  return -1;
}

#include "native_client/src/untrusted/nosys/warning.h"
stub_warning(rmdir);

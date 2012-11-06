/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


/*
 * Stub routine for `getcwd' for porting support.
 */

#include <unistd.h>
#include <errno.h>

char *getcwd(char *buf, size_t size) {
  errno = ENOSYS;
  return NULL;
}

#include "native_client/src/untrusted/nosys/warning.h"
stub_warning(getcwd);

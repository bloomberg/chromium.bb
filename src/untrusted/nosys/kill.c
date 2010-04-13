/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Stub routine for `kill' for porting support.
 */

#include <sys/types.h>
#include <signal.h>
#include <errno.h>

int kill(int pid, int sig) {
  errno = ENOSYS;
  return -1;
}

#include "native_client/src/untrusted/nosys/warning.h"
stub_warning(kill);

/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


/*
 * Stub routine for `sigprocmask' for porting support.
 */

#include <signal.h>
#include <errno.h>

int sigprocmask(int how, const sigset_t *set, sigset_t *oldset) {
  errno = ENOSYS;
  return -1;
}

#include "native_client/src/untrusted/nosys/warning.h"
stub_warning(sigprocmask);

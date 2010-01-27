/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Stub routine for kill for newlib support.
 */

#include <sys/types.h>
#include <signal.h>
#include <errno.h>

int kill(int pid, int sig) {
  /* NOT IMPLEMENTED: kill */
  errno = EINVAL;
  return -1;
}

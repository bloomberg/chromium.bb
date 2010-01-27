/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


/*
 * Stub routine for wait for newlib support.
 */

#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

int wait(int *status) {
  /* NOTE(sehr): NOT IMPLEMENTED: wait */
  errno = ECHILD;
  return -1;
}

pid_t waitpid(pid_t pid, int *status, int options) {
  errno = ECHILD;
  return -1;
}

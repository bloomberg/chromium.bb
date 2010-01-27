/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


/*
 * Stub routine for fork for newlib support.
 */

#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

pid_t fork() {
  /* NOTE(sehr): NOT IMPLEMENTED: fork */
  errno = EAGAIN;
  return -1;
}

pid_t vfork() {
  /* NOTE(sehr): NOT IMPLEMENTED: vfork */
  errno = EAGAIN;
  return -1;
}

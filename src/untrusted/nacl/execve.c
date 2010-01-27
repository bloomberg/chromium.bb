/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Stub routine for execve for newlib support.
 */

#include <unistd.h>
#include <errno.h>

int execve(const char *name, char * const argv[], char * const envp[]) {
  /* NOT IMPLEMENTED: execve */
  errno = ENOMEM;
  return -1;
}

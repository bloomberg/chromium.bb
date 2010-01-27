/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Stub routine for _execve for application porting support.
 */

#include <unistd.h>
#include <errno.h>

int _execve(const char *name, char * const argv[], char * const envp[]) {
  /* NOTE(sehr): NOT IMPLEMENTED: _execve */
  errno = ENOMEM;
  return -1;
}

/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


/*
 * Stub routines for dup/dup2 for porting support.
 */

#include <unistd.h>
#include <errno.h>

int dup(int oldfd) {
  /* NOTE(sehr): NOT IMPLEMENTED: dup */
  errno = EMFILE;
  return -1;
}

int dup2(int oldfd, int newfd) {
  /* NOTE(sehr): NOT IMPLEMENTED: dup2 */
  errno = EMFILE;
  return -1;
}

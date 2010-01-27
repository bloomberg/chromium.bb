/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


/*
 * Stub routine for pipe for porting support.
 */

#include <unistd.h>
#include <errno.h>

int pipe(int fd[2]) {
  /* NOTE(sehr): NOT IMPLEMENTED: pipe */
  errno = EMFILE;
  return -1;
}

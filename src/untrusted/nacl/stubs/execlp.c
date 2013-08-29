/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Stub routine for `execlp' for porting support.
 */

#include <unistd.h>
#include <errno.h>

int execlp(const char *file, const char *arg, ...) {
  errno = ENOSYS;
  return -1;
}

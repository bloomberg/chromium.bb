/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Stub routine for `remove' for porting support.
 */

#include <unistd.h>
#include <errno.h>

int remove(const char *path) {
  errno = ENOSYS;
  return -1;
}

/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Stub routine for `system' for porting support.
 */

#include <stdlib.h>
#include <errno.h>

int system(const char *command) {
  errno = ENOSYS;
  return -1;
}

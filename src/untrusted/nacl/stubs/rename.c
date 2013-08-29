/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Stub routine for `rename' for porting support.
 */

#include <errno.h>
#include <stdlib.h>

int rename(const char *old, const char *new) {
  errno = ENOSYS;
  return -1;
}

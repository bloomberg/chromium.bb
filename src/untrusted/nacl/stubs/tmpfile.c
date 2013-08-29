/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Stub routine for `tmpfile' for porting support.
 */

#include <errno.h>
#include <stdio.h>

FILE *tmpfile(void) {
  errno = ENOSYS;
  return NULL;
}

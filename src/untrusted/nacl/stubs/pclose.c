/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Stub routine for `pclose' for porting support.
 */

#include <stdio.h>
#include <errno.h>

int pclose(FILE *stream) {
  errno = ENOSYS;
  return -1;
}

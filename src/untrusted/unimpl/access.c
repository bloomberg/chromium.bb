/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


/*
 * Stub routine for access for porting support.
 */

#include <unistd.h>
#include <errno.h>

int access(const char *pathname, int mode) {
  /* NOTE(sehr): NOT IMPLEMENTED: access */
  errno = EACCES;
  return -1;
}

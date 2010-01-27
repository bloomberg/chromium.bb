/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


/*
 * Stub routine for rmdir for porting support.
 */

#include <unistd.h>
#include <errno.h>

int rmdir(const char *pathname) {
  /* NOTE(sehr): NOT IMPLEMENTED: rmdir */
  errno = EACCES;
  return -1;
}

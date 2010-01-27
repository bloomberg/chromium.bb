/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Stub routines to provide the lowest layer of newlib support.
 * This file is temporary until the library is moved into the development tools
 * area.
 */

#include <unistd.h>
#include <errno.h>

int link(const char *old, const char *new) {
  /* NOT IMPLEMENTED: link */
  errno = EMLINK;
  return -1;
}

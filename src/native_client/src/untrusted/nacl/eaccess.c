/*
 * Copyright 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>
#include <unistd.h>

int eaccess(const char *pathname, int mode) {
  return access(pathname, mode);
}

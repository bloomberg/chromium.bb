/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>
#include <time.h>

#include "native_client/src/untrusted/nacl/nacl_irt.h"

int clock_gettime(clockid_t clk_id, struct timespec *res) {
  int error = __libnacl_irt_clock.gettime(clk_id, res);
  if (error) {
    errno = error;
    return -1;
  }
  return 0;
}

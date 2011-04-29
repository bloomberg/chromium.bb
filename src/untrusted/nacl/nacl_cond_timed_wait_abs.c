/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/untrusted/nacl/nacl_irt.h"
#include "native_client/src/untrusted/nacl/nacl_thread.h"

int nacl_cond_timed_wait_abs(int cond_handle, int mutex_handle,
                             const struct timespec *abstime) {
  return __libnacl_irt_cond.cond_timed_wait_abs(cond_handle, mutex_handle,
                                                abstime);
}

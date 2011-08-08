/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/untrusted/irt/irt_interfaces.h"
#include "native_client/src/untrusted/pthread/pthread_internal.h"

void __nc_initialize_interfaces(struct nacl_irt_thread *irt_thread) {
  *irt_thread = nacl_irt_thread;
  __nc_irt_mutex = nacl_irt_mutex;
  __nc_irt_cond = nacl_irt_cond;
  __nc_irt_sem = nacl_irt_sem;
}

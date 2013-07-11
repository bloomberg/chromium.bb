/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/untrusted/irt/irt_futex.h"
#include "native_client/src/untrusted/nacl/nacl_irt.h"
#include "native_client/src/untrusted/pthread/pthread_internal.h"

struct nacl_irt_futex __nc_irt_futex;

void __nc_initialize_interfaces(struct nacl_irt_thread *irt_thread) {
  __libnacl_mandatory_irt_query(NACL_IRT_THREAD_v0_1,
                                irt_thread, sizeof(*irt_thread));
  __libnacl_mandatory_irt_query(NACL_IRT_FUTEX_v0_1,
                                &__nc_irt_futex, sizeof(__nc_irt_futex));
}

/*
 * In libpthread_private, there are real versions of __nc_futex_init()
 * and __nc_futex_thread_exit() implemented by irt_futex.c, called by
 * nc_thread.c.  When we are using the IRT's futex interface, however,
 * the IRT takes care of this initialization and cleanup for us.  We
 * provide no-op implementations for nc_thread.c to call.
 */
void __nc_futex_init() {
}

void __nc_futex_thread_exit() {
}

/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/untrusted/nacl/nacl_irt.h"
#include "native_client/src/untrusted/pthread/pthread_internal.h"

void __nc_initialize_interfaces(struct nacl_irt_thread *irt_thread) {
  __libnacl_mandatory_irt_query(NACL_IRT_THREAD_v0_1,
                                irt_thread, sizeof(*irt_thread));
  __libnacl_mandatory_irt_query(NACL_IRT_MUTEX_v0_1,
                                &__nc_irt_mutex, sizeof(__nc_irt_mutex));
  __libnacl_mandatory_irt_query(NACL_IRT_COND_v0_1,
                                &__nc_irt_cond, sizeof(__nc_irt_cond));
  __libnacl_mandatory_irt_query(NACL_IRT_SEM_v0_1,
                                &__nc_irt_sem, sizeof(__nc_irt_sem));
}

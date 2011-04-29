/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/untrusted/irt/irt.h"
#include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"

static int nacl_irt_tls_init(void *tdb, size_t tdb_size) {
  return -NACL_SYSCALL(tls_init)(tdb, tdb_size);
}

static void *nacl_irt_tls_get(void) {
  return NACL_SYSCALL(tls_get)();
}

const struct nacl_irt_tls nacl_irt_tls = {
  nacl_irt_tls_init,
  nacl_irt_tls_get,
};

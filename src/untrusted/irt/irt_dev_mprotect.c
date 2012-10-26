/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/untrusted/irt/irt.h"
#include "native_client/src/untrusted/irt/irt_interfaces.h"
#include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"

static int nacl_irt_mprotect(void *addr, size_t len, int prot) {
  return -NACL_SYSCALL(mprotect)(addr, len, prot);
}

const struct nacl_irt_dev_mprotect nacl_irt_dev_mprotect = {
  nacl_irt_mprotect,
};

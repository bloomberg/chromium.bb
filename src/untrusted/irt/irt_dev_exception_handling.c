/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/untrusted/irt/irt.h"
#include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"

const struct nacl_irt_dev_exception_handling nacl_irt_dev_exception_handling = {
  NACL_SYSCALL(exception_handler),
  NACL_SYSCALL(exception_stack),
  NACL_SYSCALL(exception_clear_flag),
};

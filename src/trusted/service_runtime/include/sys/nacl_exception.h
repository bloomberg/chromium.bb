/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * NaCl Exception Context
 */

#ifndef __NATIVE_CLIENT_SRC_SERVICE_RUNTIME_INCLUDE_SYS_NACL_EXCEPTION_H__
#define __NATIVE_CLIENT_SRC_SERVICE_RUNTIME_INCLUDE_SYS_NACL_EXCEPTION_H__ 1

#include "native_client/src/trusted/service_runtime/include/machine/_types.h"

struct NaClExceptionContext {
  uint32_t prog_ctr;
  uint32_t stack_ptr;
  uint32_t frame_ptr;
  /*
   * Pad this up to an even multiple of 8 bytes so this struct will add
   * a predictable amount of space to the various ExceptionFrame structures
   * used on each platform. This allows us to verify stack layout with dead
   * reckoning, without access to the ExceptionFrame structure used to set up
   * the call stack.
   */
  uint32_t pad;
};

#endif /* __NATIVE_CLIENT_SRC_SERVICE_RUNTIME_INCLUDE_SYS_NACL_EXCEPTION_H__ */

/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/trusted/service_runtime/sel_rt.h"


uintptr_t NaClGetThreadCtxSp(struct NaClThreadContext  *th_ctx) {
  return th_ctx->stack_ptr.ptr_64;
}


void NaClSetThreadCtxSp(struct NaClThreadContext  *th_ctx, uintptr_t sp) {
  th_ctx->stack_ptr.ptr_64 = (uint64_t) sp;
}


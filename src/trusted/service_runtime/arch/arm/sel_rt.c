/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * NaCl Secure Runtime
 */
#include "native_client/src/include/portability_string.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/service_runtime/sel_rt.h"
#include "native_client/src/trusted/service_runtime/arch/arm/sel_ldr_arm.h"

void NaClInitGlobals() {
   NaClLog(2, "NaClInitGlobals\n");
  /* intentionally left empty */
}


int NaClThreadContextCtor(struct NaClThreadContext  *ntcp,
                          struct NaClApp            *nap,
                          nacl_reg_t                prog_ctr,
                          nacl_reg_t                stack_ptr,
                          nacl_reg_t                tls_idx) {
  UNREFERENCED_PARAMETER(nap);

  memset((void *)ntcp, 0, sizeof(*ntcp));
  ntcp->stack_ptr = stack_ptr;
  ntcp->prog_ctr = prog_ctr;
  NaClSetThreadCombinedDescriptor(ntcp, tls_idx);

  NaClLog(4, "user.tls_idx: 0x%08"NACL_PRIxNACL_REG"\n", tls_idx);
  NaClLog(4, "user.stack_ptr: 0x%08"NACL_PRIxNACL_REG"\n", ntcp->stack_ptr);
  NaClLog(4, "user.prog_ctr: 0x%08"NACL_PRIxNACL_REG"\n", ntcp->prog_ctr);

  return 1;
}


uintptr_t NaClGetThreadCtxSp(struct NaClThreadContext  *th_ctx) {
  return (uintptr_t) th_ctx->stack_ptr;
}


void NaClSetThreadCtxSp(struct NaClThreadContext  *th_ctx, uintptr_t sp) {
  th_ctx->stack_ptr = (uint32_t) sp;
}

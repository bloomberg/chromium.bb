/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <float.h>
#include <string.h>

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/service_runtime/nacl_signal.h"
#include "native_client/src/trusted/service_runtime/nacl_app_thread.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/service_runtime/sel_rt.h"
#include "native_client/src/trusted/service_runtime/include/sys/errno.h"


uintptr_t NaClGetThreadCtxSp(struct NaClThreadContext  *th_ctx) {
  return (uintptr_t) th_ctx->rsp;
}


void NaClSetThreadCtxSp(struct NaClThreadContext  *th_ctx, uintptr_t sp) {
  th_ctx->rsp = (nacl_reg_t) sp;
}

void NaClInitGlobals(void) {
  /* no need to save segment registers */
  ;
}

int NaClThreadContextCtor(struct NaClThreadContext  *ntcp,
                          struct NaClApp            *nap,
                          nacl_reg_t                prog_ctr,
                          nacl_reg_t                stack_ptr,
                          uint32_t                  tls_idx) {
  NaClThreadContextOffsetCheck();

  memset(ntcp, 0, sizeof(*ntcp));

  ntcp->rax = 0;
  ntcp->rbx = 0;
  ntcp->rcx = 0;
  ntcp->rdx = 0;

  ntcp->rbp = stack_ptr;  /* must be a valid stack addr! */
  ntcp->rsi = 0;
  ntcp->rdi = 0;
  ntcp->rsp = stack_ptr;

  ntcp->r8 = 0;
  ntcp->r9 = 0;
  ntcp->r10 = 0;
  ntcp->r11 = 0;

  ntcp->r12 = 0;
  ntcp->r13 = 0;
  ntcp->r14 = 0;
  ntcp->r15 = nap->mem_start;

  ntcp->prog_ctr = NaClUserToSys(nap, prog_ctr);
  ntcp->new_prog_ctr = 0;
  ntcp->sysret = -NACL_ABI_EINVAL;

  ntcp->tls_idx = tls_idx;

  ntcp->fcw = NACL_X87_FCW_DEFAULT;

  /*
   * Save the system's state of the x87 FPU control word so we can restore
   * the same state when returning to trusted code.
   */
#if NACL_WINDOWS
  NaClDoFnstcw(&ntcp->sys_fcw);
#else
  __asm__ __volatile__("fnstcw %0" : "=m" (ntcp->sys_fcw));
#endif

  return 1;
}

void NaClThreadContextToSignalContext(const struct NaClThreadContext *th_ctx,
                                      struct NaClSignalContext *sig_ctx) {
  sig_ctx->rax       = th_ctx->rax;
  sig_ctx->rbx       = th_ctx->rbx;
  sig_ctx->rcx       = th_ctx->rcx;
  sig_ctx->rdx       = th_ctx->rdx;
  sig_ctx->rsi       = th_ctx->rsi;
  sig_ctx->rdi       = th_ctx->rdi;
  sig_ctx->rbp       = th_ctx->rbp;
  sig_ctx->stack_ptr = th_ctx->rsp;
  sig_ctx->r8        = th_ctx->r8;
  sig_ctx->r9        = th_ctx->r9;
  sig_ctx->r10       = th_ctx->r10;
  sig_ctx->r11       = th_ctx->r11;
  sig_ctx->r12       = th_ctx->r12;
  sig_ctx->r13       = th_ctx->r13;
  sig_ctx->r14       = th_ctx->r14;
  sig_ctx->r15       = th_ctx->r15;
  sig_ctx->prog_ctr  = th_ctx->new_prog_ctr;
  sig_ctx->flags     = 0;
  sig_ctx->cs        = 0;
  sig_ctx->ss        = 0;
  sig_ctx->ds        = 0;
  sig_ctx->es        = 0;
  sig_ctx->fs        = 0;
  sig_ctx->gs        = 0;
}

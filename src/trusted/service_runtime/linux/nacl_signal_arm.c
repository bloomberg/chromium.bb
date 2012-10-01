/*
 * Copyright (c) 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <signal.h>
#if !NACL_ANDROID
#include <sys/ucontext.h>
#endif

#include "native_client/src/trusted/service_runtime/linux/android_compat.h"
#include "native_client/src/trusted/service_runtime/nacl_signal.h"

/*
 * Fill a signal context structure from the raw platform dependent
 * signal information.
 */
void NaClSignalContextFromHandler(struct NaClSignalContext *sigCtx,
                                  const void *rawCtx) {
  ucontext_t *uctx = (ucontext_t *) rawCtx;
  struct sigcontext *mctx = &uctx->uc_mcontext;

  sigCtx->prog_ctr = mctx->arm_pc;
  sigCtx->stack_ptr = mctx->arm_sp;

  sigCtx->r0 = mctx->arm_r0;
  sigCtx->r1 = mctx->arm_r1;
  sigCtx->r2 = mctx->arm_r2;
  sigCtx->r3 = mctx->arm_r3;
  sigCtx->r4 = mctx->arm_r4;
  sigCtx->r5 = mctx->arm_r5;
  sigCtx->r6 = mctx->arm_r6;
  sigCtx->r7 = mctx->arm_r7;
  sigCtx->r8 = mctx->arm_r8;
  sigCtx->r9 = mctx->arm_r9;
  sigCtx->r10 = mctx->arm_r10;
  sigCtx->r11 = mctx->arm_fp;
  sigCtx->r12 = mctx->arm_ip;
  sigCtx->lr = mctx->arm_lr;
  sigCtx->cpsr = mctx->arm_cpsr;
}


/*
 * Update the raw platform dependent signal information from the
 * signal context structure.
 */
void NaClSignalContextToHandler(void *rawCtx,
                                const struct NaClSignalContext *sigCtx) {
  ucontext_t *uctx = (ucontext_t *) rawCtx;
  struct sigcontext *mctx = &uctx->uc_mcontext;

  mctx->arm_pc = sigCtx->prog_ctr;
  mctx->arm_sp = sigCtx->stack_ptr;

  mctx->arm_r0 = sigCtx->r0;
  mctx->arm_r1 = sigCtx->r1;
  mctx->arm_r2 = sigCtx->r2;
  mctx->arm_r3 = sigCtx->r3;
  mctx->arm_r4 = sigCtx->r4;
  mctx->arm_r5 = sigCtx->r5;
  mctx->arm_r6 = sigCtx->r6;
  mctx->arm_r7 = sigCtx->r7;
  mctx->arm_r8 = sigCtx->r8;
  mctx->arm_r9 = sigCtx->r9;
  mctx->arm_r10 = sigCtx->r10;
  mctx->arm_fp = sigCtx->r11;
  mctx->arm_ip = sigCtx->r12;
  mctx->arm_lr = sigCtx->lr;
  mctx->arm_cpsr = sigCtx->cpsr;
}

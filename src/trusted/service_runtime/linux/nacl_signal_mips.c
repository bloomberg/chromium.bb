/*
 * Copyright 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <signal.h>
#include <sys/ucontext.h>

#include "native_client/src/trusted/service_runtime/nacl_signal.h"

/*
 * Fill a signal context structure from the raw platform dependent
 * signal information.
 */
void NaClSignalContextFromHandler(struct NaClSignalContext *sigCtx,
                                  const void *rawCtx) {
  /*
   * TODO(petarj): Check whether float registers should be added here.
   */
  ucontext_t *uctx = (ucontext_t *) rawCtx;
  mcontext_t *mctx = &uctx->uc_mcontext;

  sigCtx->prog_ctr = mctx->pc;

  sigCtx->zero = mctx->gregs[0];
  sigCtx->at = mctx->gregs[1];
  sigCtx->v0 = mctx->gregs[2];
  sigCtx->v1 = mctx->gregs[3];
  sigCtx->a0 = mctx->gregs[4];
  sigCtx->a1 = mctx->gregs[5];
  sigCtx->a2 = mctx->gregs[6];
  sigCtx->a3 = mctx->gregs[7];
  sigCtx->t0 = mctx->gregs[8];
  sigCtx->t1 = mctx->gregs[9];
  sigCtx->t2 = mctx->gregs[10];
  sigCtx->t3 = mctx->gregs[11];
  sigCtx->t4 = mctx->gregs[12];
  sigCtx->t5 = mctx->gregs[13];
  sigCtx->t6 = mctx->gregs[14];
  sigCtx->t7 = mctx->gregs[15];
  sigCtx->s0 = mctx->gregs[16];
  sigCtx->s1 = mctx->gregs[17];
  sigCtx->s2 = mctx->gregs[18];
  sigCtx->s3 = mctx->gregs[19];
  sigCtx->s4 = mctx->gregs[20];
  sigCtx->s5 = mctx->gregs[21];
  sigCtx->s6 = mctx->gregs[22];
  sigCtx->s7 = mctx->gregs[23];
  sigCtx->t8 = mctx->gregs[24];
  sigCtx->t9 = mctx->gregs[25];
  sigCtx->k0 = mctx->gregs[26];
  sigCtx->k1 = mctx->gregs[27];
  sigCtx->global_ptr = mctx->gregs[28];
  sigCtx->stack_ptr = mctx->gregs[29];
  sigCtx->frame_ptr = mctx->gregs[30];
  sigCtx->return_addr = mctx->gregs[31];
}


/*
 * Update the raw platform dependent signal information from the
 * signal context structure.
 */
void NaClSignalContextToHandler(void *rawCtx,
                                const struct NaClSignalContext *sigCtx) {
  ucontext_t *uctx = (ucontext_t *) rawCtx;
  mcontext_t *mctx = &uctx->uc_mcontext;

  mctx->pc = sigCtx->prog_ctr;

  mctx->gregs[0] = sigCtx->zero;
  mctx->gregs[1] = sigCtx->at;
  mctx->gregs[2] = sigCtx->v0;
  mctx->gregs[3] = sigCtx->v1;
  mctx->gregs[4] = sigCtx->a0;
  mctx->gregs[5] = sigCtx->a1;
  mctx->gregs[6] = sigCtx->a2;
  mctx->gregs[7] = sigCtx->a3;
  mctx->gregs[8] = sigCtx->t0;
  mctx->gregs[9] = sigCtx->t1;
  mctx->gregs[10] = sigCtx->t2;
  mctx->gregs[11] = sigCtx->t3;
  mctx->gregs[12] = sigCtx->t4;
  mctx->gregs[13] = sigCtx->t5;
  mctx->gregs[14] = sigCtx->t6;
  mctx->gregs[15] = sigCtx->t7;
  mctx->gregs[16] = sigCtx->s0;
  mctx->gregs[17] = sigCtx->s1;
  mctx->gregs[18] = sigCtx->s2;
  mctx->gregs[19] = sigCtx->s3;
  mctx->gregs[20] = sigCtx->s4;
  mctx->gregs[21] = sigCtx->s5;
  mctx->gregs[22] = sigCtx->s6;
  mctx->gregs[23] = sigCtx->s7;
  mctx->gregs[24] = sigCtx->t8;
  mctx->gregs[25] = sigCtx->t9;
  mctx->gregs[26] = sigCtx->k0;
  mctx->gregs[27] = sigCtx->k1;
  mctx->gregs[28] = sigCtx->global_ptr;
  mctx->gregs[29] = sigCtx->stack_ptr;
  mctx->gregs[30] = sigCtx->frame_ptr;
  mctx->gregs[31] = sigCtx->return_addr;
}

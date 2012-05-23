/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdlib.h>
#include <stdio.h>

#include "native_client/src/include/portability.h"
#include "native_client/src/trusted/service_runtime/nacl_signal.h"

/*
 * Fill a signal context structure from the raw platform dependent
 * signal information.
 */
void NaClSignalContextFromHandler(struct NaClSignalContext *sigCtx,
                                  const void *rawCtx) {
  const CONTEXT *winCtx = rawCtx;

  sigCtx->prog_ctr = winCtx->Rip;
  sigCtx->stack_ptr = winCtx->Rsp;

  sigCtx->rax = winCtx->Rax;
  sigCtx->rbx = winCtx->Rbx;
  sigCtx->rcx = winCtx->Rcx;
  sigCtx->rdx = winCtx->Rdx;
  sigCtx->rsi = winCtx->Rsi;
  sigCtx->rdi = winCtx->Rdi;
  sigCtx->rbp = winCtx->Rbp;
  sigCtx->r8 = winCtx->R8;
  sigCtx->r9 = winCtx->R9;
  sigCtx->r10 = winCtx->R10;
  sigCtx->r11 = winCtx->R11;
  sigCtx->r12 = winCtx->R12;
  sigCtx->r13 = winCtx->R13;
  sigCtx->r14 = winCtx->R14;
  sigCtx->r15 = winCtx->R15;
  sigCtx->flags = winCtx->EFlags;
  sigCtx->cs = winCtx->SegCs;
  sigCtx->ss = winCtx->SegSs;
  sigCtx->ds = winCtx->SegDs;
  sigCtx->es = winCtx->SegEs;
  sigCtx->fs = winCtx->SegFs;
  sigCtx->gs = winCtx->SegGs;
}


/*
 * Update the raw platform dependent signal information from the
 * signal context structure.
 */
void NaClSignalContextToHandler(void *rawCtx,
                                const struct NaClSignalContext *sigCtx) {
  CONTEXT *winCtx = rawCtx;

  winCtx->Rip = sigCtx->prog_ctr;
  winCtx->Rsp = sigCtx->stack_ptr;

  winCtx->Rax = sigCtx->rax;
  winCtx->Rbx = sigCtx->rbx;
  winCtx->Rcx = sigCtx->rcx;
  winCtx->Rdx = sigCtx->rdx;
  winCtx->Rsi = sigCtx->rsi;
  winCtx->Rdi = sigCtx->rdi;
  winCtx->Rbp = sigCtx->rbp;
  winCtx->R8 = sigCtx->r8;
  winCtx->R9 = sigCtx->r9;
  winCtx->R10 = sigCtx->r10;
  winCtx->R11 = sigCtx->r11;
  winCtx->R12 = sigCtx->r12;
  winCtx->R13 = sigCtx->r13;
  winCtx->R14 = sigCtx->r14;
  winCtx->R15 = sigCtx->r15;
  winCtx->EFlags = sigCtx->flags;
  winCtx->SegCs = sigCtx->cs;
  winCtx->SegSs = sigCtx->ss;
  winCtx->SegDs = sigCtx->ds;
  winCtx->SegEs = sigCtx->es;
  winCtx->SegFs = sigCtx->fs;
  winCtx->SegGs = sigCtx->gs;
}


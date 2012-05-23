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

  sigCtx->prog_ctr = winCtx->Eip;
  sigCtx->stack_ptr = winCtx->Esp;

  sigCtx->eax = winCtx->Eax;
  sigCtx->ebx = winCtx->Ebx;
  sigCtx->ecx = winCtx->Ecx;
  sigCtx->edx = winCtx->Edx;
  sigCtx->esi = winCtx->Esi;
  sigCtx->edi = winCtx->Edi;
  sigCtx->ebp = winCtx->Ebp;
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

  winCtx->Eip = sigCtx->prog_ctr;
  winCtx->Esp = sigCtx->stack_ptr;

  winCtx->Eax = sigCtx->eax;
  winCtx->Ebx = sigCtx->ebx;
  winCtx->Ecx = sigCtx->ecx;
  winCtx->Edx = sigCtx->edx;
  winCtx->Esi = sigCtx->esi;
  winCtx->Edi = sigCtx->edi;
  winCtx->Ebp = sigCtx->ebp;
  winCtx->EFlags = sigCtx->flags;
  winCtx->SegCs = sigCtx->cs;
  winCtx->SegSs = sigCtx->ss;
  winCtx->SegDs = sigCtx->ds;
  winCtx->SegEs = sigCtx->es;
  winCtx->SegFs = sigCtx->fs;
  winCtx->SegGs = sigCtx->gs;
}

/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <signal.h>
#include <sys/ucontext.h>

#include "native_client/src/trusted/service_runtime/nacl_signal.h"

/*
 * Definition of the POSIX ucontext_t for Linux can be found in:
 * /usr/include/sys/ucontext.h
 */

/*
 * Fill a signal context structure from the raw platform dependent
 * signal information.
 */
void NaClSignalContextFromHandler(struct NaClSignalContext *sigCtx,
                                  const void *rawCtx) {
  const ucontext_t *uctx = (const ucontext_t *) rawCtx;
  const mcontext_t *mctx = &uctx->uc_mcontext;

  sigCtx->prog_ctr = mctx->gregs[REG_EIP];
  sigCtx->stack_ptr = mctx->gregs[REG_ESP];

  sigCtx->eax = mctx->gregs[REG_EAX];
  sigCtx->ebx = mctx->gregs[REG_EBX];
  sigCtx->ecx = mctx->gregs[REG_ECX];
  sigCtx->edx = mctx->gregs[REG_EDX];
  sigCtx->esi = mctx->gregs[REG_ESI];
  sigCtx->edi = mctx->gregs[REG_EDI];
  sigCtx->ebp = mctx->gregs[REG_EBP];
  sigCtx->flags = mctx->gregs[REG_EFL];
  sigCtx->cs = mctx->gregs[REG_CS];
  sigCtx->ss = mctx->gregs[REG_SS];
  sigCtx->ds = mctx->gregs[REG_DS];
  sigCtx->es = mctx->gregs[REG_ES];
  sigCtx->fs = mctx->gregs[REG_FS];
  sigCtx->gs = mctx->gregs[REG_GS];
}


/*
 * Update the raw platform dependent signal information from the
 * signal context structure.
 */
void NaClSignalContextToHandler(void *rawCtx,
                                const struct NaClSignalContext *sigCtx) {
  ucontext_t *uctx = (ucontext_t *) rawCtx;
  mcontext_t *mctx = &uctx->uc_mcontext;

  mctx->gregs[REG_EIP] = sigCtx->prog_ctr;
  mctx->gregs[REG_ESP] = sigCtx->stack_ptr;

  mctx->gregs[REG_EAX] = sigCtx->eax;
  mctx->gregs[REG_EBX] = sigCtx->ebx;
  mctx->gregs[REG_ECX] = sigCtx->ecx;
  mctx->gregs[REG_EDX] = sigCtx->edx;
  mctx->gregs[REG_ESI] = sigCtx->esi;
  mctx->gregs[REG_EDI] = sigCtx->edi;
  mctx->gregs[REG_EBP] = sigCtx->ebp;
  mctx->gregs[REG_EFL] = sigCtx->flags;
  mctx->gregs[REG_CS] = sigCtx->cs;
  mctx->gregs[REG_SS] = sigCtx->ss;
  mctx->gregs[REG_DS] = sigCtx->ds;
  mctx->gregs[REG_ES] = sigCtx->es;
  mctx->gregs[REG_FS] = sigCtx->fs;
  mctx->gregs[REG_GS] = sigCtx->gs;
}




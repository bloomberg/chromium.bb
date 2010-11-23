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
 * /usr/include/mach/i386/_structs.h
 */

#if __DARWIN_UNIX03
  #define REG(R) uc_mcontext->__ss.__##R
#else
  #define REG(R) uc_mcontext->ss.R
#endif

/*
 * Fill a signal context structure from the raw platform dependant
 * signal information.
 */
void NaClSignalContextFromHandler(struct NaClSignalContext *sigCtx,
                                  const void *rawCtx) {
  ucontext_t *uctx = (ucontext_t *) rawCtx;

  sigCtx->prog_ctr = uctx->REG(eip);
  sigCtx->stack_ptr = uctx->REG(esp);

  sigCtx->eax = uctx->REG(eax);
  sigCtx->ebx = uctx->REG(ebx);
  sigCtx->ecx = uctx->REG(ecx);
  sigCtx->edx = uctx->REG(edx);
  sigCtx->esi = uctx->REG(esi);
  sigCtx->edi = uctx->REG(edi);
  sigCtx->ebp = uctx->REG(ebp);
  sigCtx->flags = uctx->REG(eflags);
  sigCtx->cs = uctx->REG(cs);
  sigCtx->ss = uctx->REG(ss);
  sigCtx->ds = uctx->REG(ds);
  sigCtx->es = uctx->REG(es);
  sigCtx->fs = uctx->REG(fs);
  sigCtx->gs = uctx->REG(gs);
}


/*
 * Update the raw platform dependant signal information from the
 * signal context structure.
 */
void NaClSignalContextToHandler(void *rawCtx,
                                const struct NaClSignalContext *sigCtx) {
  ucontext_t *uctx = (ucontext_t *) rawCtx;

  uctx->REG(eip) = sigCtx->prog_ctr;
  uctx->REG(esp) = sigCtx->stack_ptr;

  uctx->REG(eax) = sigCtx->eax;
  uctx->REG(ebx) = sigCtx->ebx;
  uctx->REG(ecx) = sigCtx->ecx;
  uctx->REG(edx) = sigCtx->edx;
  uctx->REG(esi) = sigCtx->esi;
  uctx->REG(edi) = sigCtx->edi;
  uctx->REG(ebp) = sigCtx->ebp;
  uctx->REG(eflags) = sigCtx->flags;
  uctx->REG(cs) = sigCtx->cs;
  uctx->REG(ss) = sigCtx->ss;
  uctx->REG(ds) = sigCtx->ds;
  uctx->REG(es) = sigCtx->es;
  uctx->REG(fs) = sigCtx->fs;
  uctx->REG(gs) = sigCtx->gs;
}

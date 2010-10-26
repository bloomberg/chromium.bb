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


/* From /usr/include/mach/i386/_structs.h */

/*
 * Fill a signal context structure from the raw platform dependant
 * signal information.
 */
void NaClSignalContextFromHandler(struct NaClSignalContext *sigCtx,
                                  const void *rawCtx) {
  ucontext_t *uctx = (ucontext_t *) rawCtx;

  sigCtx->prog_ctr = uctx->REG(rIP);
  sigCtx->stack_ptr = uctx->REG(rSP);

  sigCtx->rax = uctx->REG(rax);
  sigCtx->rbx = uctx->REG(rbx);
  sigCtx->rcx = uctx->REG(rcx);
  sigCtx->rdx = uctx->REG(rdx);
  sigCtx->rsi = uctx->REG(rsi);
  sigCtx->rdi = uctx->REG(rdi);
  sigCtx->rbp = uctx->REG(rbp);
  sigCtx->r8  = uctx->REG(r8);
  sigCtx->r9  = uctx->REG(r9);
  sigCtx->r10 = uctx->REG(r10);
  sigCtx->r11 = uctx->REG(r11);
  sigCtx->r12 = uctx->REG(r12);
  sigCtx->r13 = uctx->REG(r13);
  sigCtx->r14 = uctx->REG(r14);
  sigCtx->r15 = uctx->REG(r15);
  sigCtx->flags = (unt32_t) uctx->REG(rflags);
  sigCtx->cs = (uint32_t) uctx->REG(cs);
  sigCtx->fs = (uint32_t) uctx->REG(fs);
  sigCtx->gs = (uint32_t) uctx->REG(gs);
}


/*
 * Update the raw platform dependant signal information from the
 * signal context structure.
 */
void NaClSignalContextToHandler(void *rawCtx,
                                const struct NaClSignalContext *sigCtx) {
  ucontext_t *uctx = (ucontext_t *) rawCtx;

  uctx->REG(rip) = sigCtx->prog_ctr;
  uctx->REG(rsp) = sigCtx->stack_ptr;

  uctx->REG(rax) = sigCtx->rax;
  uctx->REG(rbx) = sigCtx->rbx;
  uctx->REG(rcx) = sigCtx->rcx;
  uctx->REG(rdx) = sigCtx->rdx;
  uctx->REG(rsi) = sigCtx->rsi;
  uctx->REG(rdi) = sigCtx->rdi;
  uctx->REG(rbp) = sigCtx->rbp;
  uctx->REG(r8)  = sigCtx->r8;
  uctx->REG(r9)  = sigCtx->r9;
  uctx->REG(r10) = sigCtx->r10;
  uctx->REG(r11) = sigCtx->r11;
  uctx->REG(r12) = sigCtx->r12;
  uctx->REG(r13) = sigCtx->r13;
  uctx->REG(r14) = sigCtx->r14;
  uctx->REG(r15) = sigCtx->r15;
  uctx->REG(rflags) = sigCtx->flags;
  uctx->REG(cs) = (uint64_t) sigCtx->cs;
  uctx->REG(fs) = (uint64_t) sigCtx->fs;
  uctx->REG(gs) = (uint64_t) sigCtx->gs;
}




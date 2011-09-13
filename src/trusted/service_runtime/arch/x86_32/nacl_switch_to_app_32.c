/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * NaCl Service Runtime, C-level context switch code.
 */

#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/service_runtime/arch/x86/sel_rt.h"
#include "native_client/src/trusted/service_runtime/nacl_app_thread.h"
#include "native_client/src/trusted/service_runtime/nacl_globals.h"
#include "native_client/src/trusted/service_runtime/nacl_switch_to_app.h"
#include "native_client/src/trusted/validator/x86/nacl_cpuid.h"

#if NACL_WINDOWS
# define NORETURN_PTR
#else
# define NORETURN_PTR NORETURN
#endif

static NORETURN_PTR void (*NaClSwitch)(struct NaClThreadContext *context);

void NaClInitSwitchToApp(struct NaClApp *nap) {
  NaClCPUData cpu_data;
  CPUFeatures cpu_features;

  UNREFERENCED_PARAMETER(nap);

  /*
   * TODO(mcgrathr): This call is repeated in platform qualification and
   * in every application of the validator.  It would be more efficient
   * to do it once and then reuse the same data.
   */
  NaClCPUDataGet(&cpu_data);
  GetCPUFeatures(&cpu_data, &cpu_features);

  if (cpu_features.f_AVX) {
    NaClSwitch = NaClSwitchAVX;
  } else if (cpu_features.f_SSE) {
    NaClSwitch = NaClSwitchSSE;
  } else {
    NaClSwitch = NaClSwitchNoSSE;
  }
}

NORETURN void NaClStartThreadInApp(struct NaClAppThread *natp,
                                   nacl_reg_t           new_prog_ctr) {
  struct NaClApp            *nap;
  struct NaClThreadContext  *context;
  /*
   * Save service runtime segment registers; fs/gs is used for TLS
   * on Windows and Linux respectively, so will change.  The others
   * should be global, but we save them from the thread anyway.
   */
#if 0 /* restored by trampoline code */
  natp->sys.cs = NaClGetCs();
  natp->sys.ds = NaClGetDs();
#endif
  natp->sys.es = NaClGetEs();
  natp->sys.fs = NaClGetFs();
#if NACL_WINDOWS
  /*
   * Win32 leaks %gs values on return from a windows syscall if the
   * previously running thread had a non-zero %gs, e.g., an untrusted
   * thread was interrupted by the scheduler.  If we used NaClGetGs()
   * here, then in the trampoline context switch code, we will try to
   * restore %gs to this leaked value, possibly generating a fault
   * since that segment selector may not be valid (e.g., if that
   * earlier thread had exited and its selector had been deallocated).
   */
  natp->sys.gs = 0;
#else
  natp->sys.gs = NaClGetGs();
#endif
  natp->sys.ss = NaClGetSs();
  /*
   * Preserves stack alignment.  The trampoline code loads this value
   * to %esp, then pushes the thread ID (LDT index) onto the stack as
   * argument to NaClSyscallCSegHook.  See nacl_syscall.S.
   */
  NaClSetThreadCtxSp(&natp->sys, (NaClGetStackPtr() & ~0xf) + 4);

  nap = natp->nap;
  context = &natp->user;
  context->spring_addr = NaClSysToUser(nap,
                                       nap->mem_start + nap->springboard_addr);
  context->new_prog_ctr = new_prog_ctr;
  context->sysret = 0; /* %eax not used to return */

  NaClSwitch(context);
}


/*
 * syscall return
 */
NORETURN void NaClSwitchToApp(struct NaClAppThread *natp,
                              nacl_reg_t           new_prog_ctr) {
  struct NaClApp            *nap;
  struct NaClThreadContext  *context;

  nap = natp->nap;
  context = &natp->user;
  context->new_prog_ctr = new_prog_ctr;
  context->sysret = natp->sysret;

  NaClSwitch(context);
}

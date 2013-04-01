/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "native_client/src/include/nacl_base.h"
#include "native_client/src/include/portability_io.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_exit.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/service_runtime/nacl_app_thread.h"
#include "native_client/src/trusted/service_runtime/nacl_globals.h"
#include "native_client/src/trusted/service_runtime/nacl_signal.h"
#include "native_client/src/trusted/service_runtime/nacl_tls.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"

#if NACL_WINDOWS
#include <io.h>
#define write _write
#else
#include <unistd.h>
#endif


ssize_t NaClSignalErrorMessage(const char *msg) {
  /*
   * We cannot use NaClLog() in the context of a signal handler: it is
   * too complex.  However, write() is signal-safe.
   */
  size_t len_t = strlen(msg);
  int len = (int) len_t;

  /*
   * Write uses int not size_t, so we may wrap the length and/or
   * generate a negative value.  Only print if it matches.
   */
  if ((len > 0) && (len_t == (size_t) len)) {
    return (ssize_t) write(2, msg, len);
  }

  return 0;
}

/*
 * Returns, via is_untrusted, whether the signal happened while
 * executing untrusted code.
 *
 * Returns, via result_thread, the NaClAppThread that untrusted code
 * was running in.
 *
 * Note that this should only be called from the thread in which the
 * signal occurred, because on x86-64 it reads a thread-local variable
 * (nacl_current_thread).
 */
void NaClSignalContextGetCurrentThread(const struct NaClSignalContext *sig_ctx,
                                       int *is_untrusted,
                                       struct NaClAppThread **result_thread) {
#if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 && NACL_BUILD_SUBARCH == 32
  /*
   * For x86-32, if %cs does not match, it is untrusted code.
   *
   * Note that this check may not be valid on Mac OS X, because
   * thread_get_state() does not return the value of NaClGetGlobalCs()
   * for a thread suspended inside a syscall.
   * TODO(mseaborn): Don't define this function on Mac OS X.  We can
   * compile this conditionally when NaCl's POSIX signal handler is no
   * longer built for Mac.
   * See https://code.google.com/p/nativeclient/issues/detail?id=2664
   */
  *is_untrusted = (NaClGetGlobalCs() != sig_ctx->cs);
  *result_thread = NaClAppThreadGetFromIndex(sig_ctx->gs >> 3);
#elif (NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 && NACL_BUILD_SUBARCH == 64) || \
      NACL_ARCH(NACL_BUILD_ARCH) == NACL_arm || \
      NACL_ARCH(NACL_BUILD_ARCH) == NACL_mips
  struct NaClAppThread *natp = NaClTlsGetCurrentThread();
  if (natp == NULL) {
    *is_untrusted = 0;
    *result_thread = NULL;
  } else {
    /*
     * Get the address of an arbitrary local, stack-allocated variable,
     * just for the purpose of doing a sanity check.
     */
    void *pointer_into_stack = &natp;
    /*
     * Sanity check: Make sure the stack we are running on is not
     * allocated in untrusted memory.  This checks that the alternate
     * signal stack is correctly set up, because otherwise, if it is
     * not set up, the test case would not detect that.
     *
     * There is little point in doing a CHECK instead of a DCHECK,
     * because if we are running off an untrusted stack, we have already
     * lost.
     *
     * We do not do the check on Windows because Windows does not have
     * an equivalent of sigaltstack() and this signal handler is
     * insecure there.
     */
    if (!NACL_WINDOWS) {
      DCHECK(!NaClIsUserAddr(natp->nap, (uintptr_t) pointer_into_stack));
    }
    *is_untrusted = NaClIsUserAddr(natp->nap, sig_ctx->prog_ctr);
    *result_thread = natp;
  }
#else
# error Unsupported architecture
#endif

  /*
   * Trusted code could accidentally jump into sandbox address space,
   * so don't rely on prog_ctr on its own for determining whether a
   * crash comes from untrusted code.  We don't want to restore
   * control to an untrusted exception handler if trusted code
   * crashes.
   */
  if (*is_untrusted &&
      ((*result_thread)->suspend_state & NACL_APP_THREAD_UNTRUSTED) == 0) {
    *is_untrusted = 0;
  }
}

/*
 * Returns whether the signal happened while executing untrusted code.
 *
 * Like NaClSignalContextGetCurrentThread(), this should only be
 * called from the thread in which the signal occurred.
 */
int NaClSignalContextIsUntrustedForCurrentThread(
    const struct NaClSignalContext *sig_ctx) {
  struct NaClAppThread *thread_unused;
  int is_untrusted;
  NaClSignalContextGetCurrentThread(sig_ctx, &is_untrusted, &thread_unused);
  return is_untrusted;
}

/*
 * This function takes the register state (sig_ctx) for a thread
 * (natp) that has been suspended and returns whether the thread was
 * suspended while executing untrusted code.
 */
int NaClSignalContextIsUntrusted(struct NaClAppThread *natp,
                                 const struct NaClSignalContext *sig_ctx) {
  uint32_t prog_ctr;

#if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 && NACL_BUILD_SUBARCH == 32
  /*
   * Note that we do not check "sig_ctx != NaClGetGlobalCs()".  On Mac
   * OS X, if a thread is suspended while in a syscall,
   * thread_get_state() returns cs=0x7 rather than cs=0x17 (the normal
   * cs value for trusted code).
   */
  if (sig_ctx->cs != natp->user.cs)
    return 0;
#elif (NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 && NACL_BUILD_SUBARCH == 64) || \
      NACL_ARCH(NACL_BUILD_ARCH) == NACL_arm || \
      NACL_ARCH(NACL_BUILD_ARCH) == NACL_mips
  if (!NaClIsUserAddr(natp->nap, sig_ctx->prog_ctr))
    return 0;
#else
# error Unsupported architecture
#endif

  prog_ctr = (uint32_t) sig_ctx->prog_ctr;
  return (prog_ctr < NACL_TRAMPOLINE_START ||
          prog_ctr >= NACL_TRAMPOLINE_END);
}

void NaClSignalHandleUntrusted(int signal, void *ctx) {
  struct NaClSignalContext sig_ctx;
  char tmp[128];
  /*
   * Return an 8 bit error code which is -signal to
   * simulate normal OS behavior
   */
  NaClSignalContextFromHandler(&sig_ctx, ctx);
  if (NaClSignalContextIsUntrustedForCurrentThread(&sig_ctx)) {
    SNPRINTF(tmp, sizeof(tmp), "\n** Signal %d from untrusted code: "
             "pc=%" NACL_PRIxNACL_REG "\n", signal, sig_ctx.prog_ctr);
    NaClSignalErrorMessage(tmp);
    NaClExit((-signal) & 0xFF);
  }
  else {
    SNPRINTF(tmp, sizeof(tmp), "\n** Signal %d from trusted code: "
             "pc=%" NACL_PRIxNACL_REG "\n", signal, sig_ctx.prog_ctr);
    NaClSignalErrorMessage(tmp);
    /*
     * Continue the search for another handler so that trusted crashes
     * can be handled by the Breakpad crash reporter.
     */
  }
}


/*
 * This is a separate function to make it obvious from the crash
 * reports that this crash is deliberate and for testing purposes.
 */
void NaClSignalTestCrashOnStartup(void) {
  if (getenv("NACL_CRASH_TEST") != NULL) {
    NaClSignalErrorMessage("[CRASH_TEST] Causing crash in NaCl "
                           "trusted code...\n");
    /*
     * Clang transmutes a NULL pointer reference into a generic
     * "undefined" case.  That code crashes with a different signal
     * than an actual bad pointer reference, violating the tests'
     * expectations.  A pointer that is known bad but is not literally
     * NULL does not get this treatment.
     */
    *(volatile int *) 1 = 0;
  }
}

void NaClSignalHandlerInit(void) {
  NaClSignalHandlerInitPlatform();
}

void NaClSignalHandlerFini(void) {
  /* We try to lock, but since we are shutting down, we ignore failures. */
  NaClSignalHandlerFiniPlatform();
}

void NaClUserRegisterStateFromSignalContext(
    volatile NaClUserRegisterState *dest,
    const struct NaClSignalContext *src) {
#define COPY_REG(reg) dest->reg = src->reg
#if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 && NACL_BUILD_SUBARCH == 32
  COPY_REG(eax);
  COPY_REG(ecx);
  COPY_REG(edx);
  COPY_REG(ebx);
  COPY_REG(stack_ptr);
  COPY_REG(ebp);
  COPY_REG(esi);
  COPY_REG(edi);
  COPY_REG(prog_ctr);
  COPY_REG(flags);
#elif NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 && NACL_BUILD_SUBARCH == 64
  COPY_REG(rax);
  COPY_REG(rcx);
  COPY_REG(rdx);
  COPY_REG(rbx);
  COPY_REG(stack_ptr);
  COPY_REG(rbp);
  COPY_REG(rsi);
  COPY_REG(rdi);
  COPY_REG(r8);
  COPY_REG(r9);
  COPY_REG(r10);
  COPY_REG(r11);
  COPY_REG(r12);
  COPY_REG(r13);
  COPY_REG(r14);
  COPY_REG(r15);
  COPY_REG(prog_ctr);
  COPY_REG(flags);
#elif NACL_ARCH(NACL_BUILD_ARCH) == NACL_arm
  COPY_REG(r0);
  COPY_REG(r1);
  COPY_REG(r2);
  COPY_REG(r3);
  COPY_REG(r4);
  COPY_REG(r5);
  COPY_REG(r6);
  COPY_REG(r7);
  COPY_REG(r8);
  COPY_REG(r9);
  COPY_REG(r10);
  COPY_REG(r11);
  COPY_REG(r12);
  COPY_REG(stack_ptr);
  COPY_REG(lr);
  COPY_REG(prog_ctr);
  COPY_REG(cpsr);
#elif NACL_ARCH(NACL_BUILD_ARCH) == NACL_mips
  COPY_REG(zero);
  COPY_REG(at);
  COPY_REG(v0);
  COPY_REG(v1);
  COPY_REG(a0);
  COPY_REG(a1);
  COPY_REG(a2);
  COPY_REG(a3);
  COPY_REG(t0);
  COPY_REG(t1);
  COPY_REG(t2);
  COPY_REG(t3);
  COPY_REG(t4);
  COPY_REG(t5);
  COPY_REG(t6);
  COPY_REG(t7);
  COPY_REG(s0);
  COPY_REG(s1);
  COPY_REG(s2);
  COPY_REG(s3);
  COPY_REG(s4);
  COPY_REG(s5);
  COPY_REG(s6);
  COPY_REG(s7);
  COPY_REG(t8);
  COPY_REG(t9);
  COPY_REG(k0);
  COPY_REG(k1);
  COPY_REG(global_ptr);
  COPY_REG(stack_ptr);
  COPY_REG(frame_ptr);
  COPY_REG(return_addr);
  COPY_REG(prog_ctr);
#else
# error Unsupported architecture
#endif
#undef COPY_REG
}

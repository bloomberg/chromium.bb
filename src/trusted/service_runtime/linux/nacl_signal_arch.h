/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SERVICE_RUNTIME_NACL_SIGNAL_ARCH_H__
#define NATIVE_CLIENT_SERVICE_RUNTIME_NACL_SIGNAL_ARCH_H__ 1


/*
 * Recovers from a fault that occurred in trusted or untrusted code,
 * restoring TLS registers on systems where this is necessary.
 * Returns whether the fault occurred in untrusted code via
 * in_untrusted_code.
 */
static INLINE void NaClRecoverFromSignal(struct NaClApp *nap,
                                         ucontext_t *context,
                                         int *in_untrusted_code);


#if (NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 &&  \
     NACL_BUILD_SUBARCH == 32)

static INLINE void NaClRecoverFromSignal(struct NaClApp *nap,
                                         ucontext_t *context,
                                         int *in_untrusted_code) {
  /*
   * We need to drop the top 16 bits with this implicit cast.  In some
   * situations, Linux does not assign to the top 2 bytes of the
   * REG_CS array entry when writing %cs to the stack.  Therefore we
   * need to drop the undefined top 2 bytes.  This happens in 32-bit
   * processes running on 64-bit kernels, but not on 32-bit kernels.
   */
  uint16_t cs = context->uc_mcontext.gregs[REG_CS];
  UNREFERENCED_PARAMETER(nap);

  *in_untrusted_code = cs != NaClGetGlobalCs();
  if (*in_untrusted_code) {
    /*
     * We need to restore %gs before we can make any libc calls,
     * because some builds of glibc fetch a syscall function pointer
     * from glibc's static TLS area.
     *
     * Note that, in comparison, Breakpad tries to avoid using libc
     * calls at all when a crash occurs.
     */
    uint16_t guest_gs = context->uc_mcontext.gregs[REG_GS];
    struct NaClThreadContext *nacl_thread = nacl_sys[guest_gs >> 3];
    NaClSetGs(nacl_thread->gs);
  }
}

#elif (NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 && \
       NACL_BUILD_SUBARCH == 64)

static INLINE void NaClRecoverFromSignal(struct NaClApp *nap,
                                         ucontext_t *context,
                                         int *in_untrusted_code) {
  uintptr_t rip = context->uc_mcontext.gregs[REG_RIP];
  *in_untrusted_code = nap != NULL && NaClIsUserAddr(nap, rip);
}

#else
# error "Unsupported system"
#endif


static INLINE void NaClSignalExit() {
  /*
   * This is a sure fire, simple way of ensuring the process dies.
   * This assumes we're handling a SIGSEGV, in which case SIGSEGV is
   * blocked and the signal handler won't be re-entered.
   *
   * tgkill() on its own inside the signal handler is no good because
   * the signal is blocked and tgkill() will do nothing.  Unblocking
   * the signal is no good because tgkill() would re-enter the signal
   * handler.  We would have to unregister the signal handler.
   */
  __asm__("hlt");
}


#endif  /* NATIVE_CLIENT_SERVICE_RUNTIME_NACL_SIGNAL_ARCH_H__ */

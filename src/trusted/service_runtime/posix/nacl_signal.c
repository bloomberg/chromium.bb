/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>
#include <signal.h>
#include <stddef.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_exit.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/service_runtime/arch/sel_ldr_arch.h"
#include "native_client/src/trusted/service_runtime/nacl_app_thread.h"
#include "native_client/src/trusted/service_runtime/nacl_config.h"
#include "native_client/src/trusted/service_runtime/nacl_exception.h"
#include "native_client/src/trusted/service_runtime/nacl_globals.h"
#include "native_client/src/trusted/service_runtime/nacl_signal.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/service_runtime/sel_rt.h"
#include "native_client/src/trusted/service_runtime/thread_suspension.h"


/*
 * This module is based on the Posix signal model.  See:
 * http://www.opengroup.org/onlinepubs/009695399/functions/sigaction.html
 */

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif

/*
 * TODO(noelallen) split these macros and conditional compiles
 * into architecture specific files. Bug #955
 */

/* Use 4K more than the minimum to allow breakpad to run. */
#define SIGNAL_STACK_SIZE (SIGSTKSZ + 4096)
#define STACK_GUARD_SIZE NACL_PAGESIZE

static int s_Signals[] = {
#if NACL_LINUX
# if NACL_ARCH(NACL_BUILD_ARCH) != NACL_mips
  /* This signal does not exist on MIPS. */
  SIGSTKFLT,
# endif
  NACL_THREAD_SUSPEND_SIGNAL,
#endif
  SIGINT, SIGQUIT, SIGILL, SIGTRAP, SIGBUS, SIGFPE, SIGSEGV,
  /* Handle SIGABRT in case someone sends it asynchronously using kill(). */
  SIGABRT
};

static struct sigaction s_OldActions[NACL_ARRAY_SIZE_UNSAFE(s_Signals)];

int NaClSignalStackAllocate(void **result) {
  /*
   * We use mmap() to allocate the signal stack for two reasons:
   *
   * 1) By page-aligning the memory allocation (which malloc() does
   * not do for small allocations), we avoid allocating any real
   * memory in the common case in which the signal handler is never
   * run.
   *
   * 2) We get to create a guard page, to guard against the unlikely
   * occurrence of the signal handler both overrunning and doing so in
   * an exploitable way.
   */
  uint8_t *stack = mmap(NULL, SIGNAL_STACK_SIZE + STACK_GUARD_SIZE,
                        PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS,
                        -1, 0);
  if (stack == MAP_FAILED) {
    return 0;
  }
  /* We assume that the stack grows downwards. */
  if (mprotect(stack, STACK_GUARD_SIZE, PROT_NONE) != 0) {
    NaClLog(LOG_FATAL, "Failed to mprotect() the stack guard page:\n\t%s\n",
      strerror(errno));
  }
  *result = stack;
  return 1;
}

void NaClSignalStackFree(void *stack) {
  CHECK(stack != NULL);
  if (munmap(stack, SIGNAL_STACK_SIZE + STACK_GUARD_SIZE) != 0) {
    NaClLog(LOG_FATAL, "Failed to munmap() signal stack:\n\t%s\n",
      strerror(errno));
  }
}

void NaClSignalStackRegister(void *stack) {
  /*
   * If we set up signal handlers, we must ensure that any thread that
   * runs untrusted code has an alternate signal stack set up.  The
   * default for a new thread is to use the stack pointer from the
   * point at which the fault occurs, but it would not be safe to use
   * untrusted code's %esp/%rsp value.
   */
  stack_t st;
  st.ss_size = SIGNAL_STACK_SIZE;
  st.ss_sp = ((uint8_t *) stack) + STACK_GUARD_SIZE;
  st.ss_flags = 0;
  if (sigaltstack(&st, NULL) != 0) {
    NaClLog(LOG_FATAL, "Failed to register signal stack:\n\t%s\n",
      strerror(errno));
  }
}

void NaClSignalStackUnregister(void) {
  /*
   * Unregister the signal stack in case a fault occurs between the
   * thread deallocating the signal stack and exiting.  Such a fault
   * could be unsafe if the address space were reallocated before the
   * fault, although that is unlikely.
   */
  stack_t st;
#if NACL_OSX
  /*
   * This is a workaround for a bug in Mac OS X's libc, in which new
   * versions of the sigaltstack() wrapper return ENOMEM if ss_size is
   * less than MINSIGSTKSZ, even when ss_size should be ignored
   * because we are unregistering the signal stack.
   * See http://code.google.com/p/nativeclient/issues/detail?id=1053
   */
  st.ss_size = MINSIGSTKSZ;
#else
  st.ss_size = 0;
#endif
  st.ss_sp = NULL;
  st.ss_flags = SS_DISABLE;
  if (sigaltstack(&st, NULL) != 0) {
    NaClLog(LOG_FATAL, "Failed to unregister signal stack:\n\t%s\n",
      strerror(errno));
  }
}

static void FindAndRunHandler(int sig, siginfo_t *info, void *uc) {
  if (NaClSignalHandlerFind(sig, uc) == NACL_SIGNAL_SEARCH) {
    unsigned int a;

    /* If we need to keep searching, try the old signal handler. */
    for (a = 0; a < NACL_ARRAY_SIZE(s_Signals); a++) {
      /* If we handle this signal */
      if (s_Signals[a] == sig) {
        /* If this is a real sigaction pointer... */
        if ((s_OldActions[a].sa_flags & SA_SIGINFO) != 0) {
          /*
           * On Mac OS X, sigaction() can return a "struct sigaction"
           * with SA_SIGINFO set but with a NULL sa_sigaction if no
           * signal handler was previously registered.  This is
           * allowed by POSIX, which does not require a struct
           * returned by sigaction() to be intelligible.  We check for
           * NULL here to avoid a crash.
           */
          if (s_OldActions[a].sa_sigaction != NULL) {
            /* then call the old handler. */
            s_OldActions[a].sa_sigaction(sig, info, uc);
            break;
          }
        } else {
          /* otherwise check if it is a real signal pointer */
          if ((s_OldActions[a].sa_handler != SIG_DFL) &&
              (s_OldActions[a].sa_handler != SIG_IGN)) {
            /* and call the old signal. */
            s_OldActions[a].sa_handler(sig);
            break;
          }
        }
        /*
         * We matched the signal, but didn't handle it, so we emulate
         * the default behavior which is to exit the app with the signal
         * number as the error code.
         */
        NaClExit(-sig);
      }
    }
  }
}

/*
 * This function checks whether we can dispatch the signal to an
 * untrusted exception handler.  If we can, it modifies the register
 * state to call the handler and writes a stack frame into into
 * untrusted address space, and returns true.  Otherwise, it returns
 * false.
 */
static int DispatchToUntrustedHandler(struct NaClAppThread *natp,
                                      struct NaClSignalContext *regs) {
  struct NaClApp *nap = natp->nap;
  uintptr_t frame_addr;
  volatile struct NaClExceptionFrame *frame;
  uint32_t new_stack_ptr;
  uintptr_t context_user_addr;
  /*
   * Returning from the exception handler is not possible, so to avoid
   * any confusion that might arise from jumping to an uninitialised
   * address, we set the return address to zero.
   */
  const uint32_t kReturnAddr = 0;

  if (nap->exception_handler == 0) {
    return 0;
  }
  if (natp->exception_flag) {
    return 0;
  }

  natp->exception_flag = 1;

  if (natp->exception_stack == 0) {
    new_stack_ptr = regs->stack_ptr - NACL_STACK_RED_ZONE;
  } else {
    new_stack_ptr = natp->exception_stack;
  }
  /* Allocate space for the stack frame, and ensure its alignment. */
  new_stack_ptr -=
      sizeof(struct NaClExceptionFrame) - NACL_STACK_PAD_BELOW_ALIGN;
  new_stack_ptr = new_stack_ptr & ~NACL_STACK_ALIGN_MASK;
  new_stack_ptr -= NACL_STACK_PAD_BELOW_ALIGN;
  frame_addr = NaClUserToSysAddrRange(nap, new_stack_ptr,
                                      sizeof(struct NaClExceptionFrame));
  if (frame_addr == kNaClBadAddress) {
    /* We cannot write the stack frame. */
    return 0;
  }
  context_user_addr = new_stack_ptr + offsetof(struct NaClExceptionFrame,
                                               context);

  frame = (struct NaClExceptionFrame *) frame_addr;
  frame->context.prog_ctr = (uint32_t) regs->prog_ctr;
  frame->context.stack_ptr = (uint32_t) regs->stack_ptr;
  NaClUserRegisterStateFromSignalContext(&frame->context.regs, regs);

#if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 && NACL_BUILD_SUBARCH == 32
  frame->context_ptr = context_user_addr;
  frame->context.frame_ptr = regs->ebp;
  regs->prog_ctr = nap->exception_handler;
  regs->stack_ptr = new_stack_ptr;
#elif NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 && NACL_BUILD_SUBARCH == 64
  frame->context.frame_ptr = regs->rbp;
  regs->rdi = context_user_addr; /* Argument 1 */
  regs->prog_ctr = NaClUserToSys(nap, nap->exception_handler);
  regs->stack_ptr = NaClUserToSys(nap, new_stack_ptr);
  /*
   * We cannot leave %rbp unmodified because the x86-64 sandbox allows
   * %rbp to point temporarily to the lower 4GB of address space, and
   * we could have received an asynchronous signal while %rbp is in
   * this state.  Even SIGSEGV can be asynchronous if sent with
   * kill().
   *
   * For now, reset %rbp to zero in untrusted address space.  In the
   * future, we might want to allow the stack to be unwound past the
   * exception frame, and so we might want to treat %rbp differently.
   */
  regs->rbp = nap->mem_start;
#elif NACL_ARCH(NACL_BUILD_ARCH) == NACL_arm
  frame->context.frame_ptr = regs->r11;
  regs->lr = kReturnAddr;
  regs->r0 = context_user_addr;  /* Argument 1 */
  regs->prog_ctr = NaClUserToSys(nap, nap->exception_handler);
  regs->stack_ptr = NaClUserToSys(nap, new_stack_ptr);
#elif NACL_ARCH(NACL_BUILD_ARCH) == NACL_mips
  frame->context.frame_ptr = regs->frame_ptr;
  regs->return_addr = kReturnAddr;
  regs->a0 = context_user_addr;
  regs->prog_ctr = NaClUserToSys(nap, nap->exception_handler);
  regs->stack_ptr = NaClUserToSys(nap, new_stack_ptr);
#else
# error Unsupported architecture
#endif

#if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86
  frame->return_addr = kReturnAddr;
  regs->flags &= ~NACL_X86_DIRECTION_FLAG;
#endif

  return 1;
}

static void SignalCatch(int sig, siginfo_t *info, void *uc) {
  struct NaClSignalContext sig_ctx;
  int is_untrusted;
  struct NaClAppThread *natp;

#if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86
  /*
   * Reset the x86 direction flag.  New versions of gcc and libc
   * assume that the direction flag is clear on entry to a function,
   * as the x86 ABI requires.  However, untrusted code can set this
   * flag, and versions of Linux before 2.6.25 do not clear the flag
   * before running the signal handler, so we clear it here for safety.
   * See http://code.google.com/p/nativeclient/issues/detail?id=1495
   */
  __asm__("cld");
#endif

  NaClSignalContextFromHandler(&sig_ctx, uc);
  NaClSignalContextGetCurrentThread(&sig_ctx, &is_untrusted, &natp);

#if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 && NACL_BUILD_SUBARCH == 32
  /*
   * On Linux, the kernel does not restore %gs when entering the
   * signal handler, so we must do that here.  We need to do this for
   * TLS to work and for glibc's syscall wrappers to work, because
   * some builds of glibc fetch a syscall function pointer from the
   * static TLS area.  There is the potential for vulnerabilities if
   * we call glibc without restoring %gs (such as
   * http://code.google.com/p/nativeclient/issues/detail?id=1607),
   * although the risk is reduced because the untrusted %gs segment
   * has an extent of only 4 bytes (see
   * http://code.google.com/p/nativeclient/issues/detail?id=2176).
   *
   * Note that, in comparison, Breakpad tries to avoid using libc
   * calls at all when a crash occurs.
   *
   * On Mac OS X, the kernel *does* restore the original %gs when
   * entering the signal handler.  Our assignment to %gs here is
   * therefore not strictly necessary, but not harmful.  However, this
   * does mean we need to check the original %gs value (from the
   * signal frame) rather than the current %gs value (from
   * NaClGetGs()).
   *
   * Both systems necessarily restore %cs, %ds, and %ss otherwise we
   * would have a hard time handling signals in untrusted code at all.
   *
   * Note that we check natp (which is based on %gs) rather than
   * is_untrusted (which is based on %cs) because we need to handle
   * the case where %gs is set to the untrusted-code value but %cs is
   * not.
   */
  if (natp != NULL) {
    NaClSetGs(natp->user.trusted_gs);
  }
#endif

#if NACL_LINUX
  if (sig != SIGINT && sig != SIGQUIT) {
    if (NaClThreadSuspensionSignalHandler(sig, &sig_ctx, is_untrusted, natp)) {
      NaClSignalContextToHandler(uc, &sig_ctx);
      /* Resume untrusted code using possibly modified register state. */
      return;
    }
  }
#endif

  if (is_untrusted && sig == SIGSEGV) {
    if (DispatchToUntrustedHandler(natp, &sig_ctx)) {
      NaClSignalContextToHandler(uc, &sig_ctx);
      /* Resume untrusted code using the modified register state. */
      return;
    }
  }

  FindAndRunHandler(sig, info, uc);
}


/*
 * Check that the current process has no signal handlers registered
 * that we won't override with safe handlers.
 *
 * We want to discourage Chrome or libraries from registering signal
 * handlers themselves, because those signal handlers are often not
 * safe when triggered from untrusted code.  For background, see:
 * http://code.google.com/p/nativeclient/issues/detail?id=1607
 */
static void AssertNoOtherSignalHandlers(void) {
  unsigned int index;
  int signum;
  char handled_by_nacl[NSIG];

  /* 0 is not a valid signal number. */
  for (signum = 1; signum < NSIG; signum++) {
    handled_by_nacl[signum] = 0;
  }
  for (index = 0; index < NACL_ARRAY_SIZE(s_Signals); index++) {
    signum = s_Signals[index];
    CHECK(signum > 0);
    CHECK(signum < NSIG);
    handled_by_nacl[signum] = 1;
  }
  for (signum = 1; signum < NSIG; signum++) {
    struct sigaction sa;

    if (handled_by_nacl[signum])
      continue;

    if (sigaction(signum, NULL, &sa) != 0) {
      /*
       * Don't complain if the kernel does not consider signum to be a
       * valid signal number, which produces EINVAL.
       */
      if (errno != EINVAL) {
        NaClLog(LOG_FATAL, "NaClSignalHandlerInitPlatform: "
                "sigaction() call failed for signal %d: errno=%d\n",
                signum, errno);
      }
    } else {
      if ((sa.sa_flags & SA_SIGINFO) == 0) {
        if (sa.sa_handler == SIG_DFL || sa.sa_handler == SIG_IGN)
          continue;
      } else {
        /*
         * It is not strictly legal for sa_sigaction to contain NULL
         * or SIG_IGN, but Valgrind reports SIG_IGN for signal 64, so
         * we allow it here.
         */
        if (sa.sa_sigaction == NULL ||
            sa.sa_sigaction == (void (*)(int, siginfo_t *, void *)) SIG_IGN)
          continue;
      }
      NaClLog(LOG_FATAL, "NaClSignalHandlerInitPlatform: "
              "A signal handler is registered for signal %d\n", signum);
    }
  }
}

void NaClSignalHandlerInitPlatform(void) {
  struct sigaction sa;
  unsigned int a;

  AssertNoOtherSignalHandlers();

  memset(&sa, 0, sizeof(sa));
  sigemptyset(&sa.sa_mask);
  sa.sa_sigaction = SignalCatch;
  sa.sa_flags = SA_ONSTACK | SA_SIGINFO;

  /*
   * Mask all signals we catch to prevent re-entry.
   *
   * In particular, NACL_THREAD_SUSPEND_SIGNAL must be masked while we
   * are handling a fault from untrusted code, otherwise the
   * suspension signal will interrupt the trusted fault handler.  That
   * would cause NaClAppThreadGetSuspendedRegisters() to report
   * trusted-code register state rather than untrusted-code register
   * state from the point where the fault occurred.
   */
  for (a = 0; a < NACL_ARRAY_SIZE(s_Signals); a++) {
    sigaddset(&sa.sa_mask, s_Signals[a]);
  }

  /* Install all handlers */
  for (a = 0; a < NACL_ARRAY_SIZE(s_Signals); a++) {
    if (sigaction(s_Signals[a], &sa, &s_OldActions[a]) != 0) {
      NaClLog(LOG_FATAL, "Failed to install handler for %d.\n\tERR:%s\n",
                          s_Signals[a], strerror(errno));
    }
  }
}

void NaClSignalHandlerFiniPlatform(void) {
  unsigned int a;

  /* Remove all handlers */
  for (a = 0; a < NACL_ARRAY_SIZE(s_Signals); a++) {
    if (sigaction(s_Signals[a], &s_OldActions[a], NULL) != 0) {
      NaClLog(LOG_FATAL, "Failed to unregister handler for %d.\n\tERR:%s\n",
                          s_Signals[a], strerror(errno));
    }
  }
}

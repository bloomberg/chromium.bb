/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_exit.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/service_runtime/arch/sel_ldr_arch.h"
#include "native_client/src/trusted/service_runtime/nacl_app_thread.h"
#include "native_client/src/trusted/service_runtime/nacl_config.h"
#include "native_client/src/trusted/service_runtime/nacl_globals.h"
#include "native_client/src/trusted/service_runtime/nacl_signal.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/service_runtime/sel_rt.h"


struct ExceptionFrame {
#if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86
  nacl_reg_t return_addr;
# if NACL_BUILD_SUBARCH == 32
  uint32_t prog_ctr; /* Function argument 1 */
  uint32_t stack_ptr; /* Function argument 2 */
# endif
#elif NACL_ARCH(NACL_BUILD_ARCH) == NACL_arm
  /*
   * Empty structs are a corner case and can have different sizeof()s
   * in C and C++, so to avoid potential confusion we have a dummy
   * entry here.
   * TODO(mseaborn): Eventually we will push a proper stack frame
   * containing a full register dump, so this dummy field will go
   * away.
   */
  uint32_t dummy;
#else
# error Unknown architecture
#endif
};

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

#ifdef SIGSTKFLT
#define SIGNAL_COUNT 8
static int s_Signals[SIGNAL_COUNT] = {
  SIGINT, SIGQUIT, SIGILL, SIGTRAP, SIGBUS, SIGFPE, SIGSEGV, SIGSTKFLT
};
#else
#define SIGNAL_COUNT 7
static int s_Signals[SIGNAL_COUNT] = {
  SIGINT, SIGQUIT, SIGILL, SIGTRAP, SIGBUS, SIGFPE, SIGSEGV
};
#endif

static struct sigaction s_OldActions[SIGNAL_COUNT];

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
    int a;

    /* If we need to keep searching, try the old signal handler. */
    for (a = 0; a < SIGNAL_COUNT; a++) {
      /* If we handle this signal */
      if (s_Signals[a] == sig) {
        /* If this is a real sigaction pointer... */
        if (s_OldActions[a].sa_flags & SA_SIGINFO) {
          /* then call the old handler. */
          s_OldActions[a].sa_sigaction(sig, info, uc);
          break;
        }
        /* otherwise check if it is a real signal pointer */
        if ((s_OldActions[a].sa_handler != SIG_DFL) &&
            (s_OldActions[a].sa_handler != SIG_IGN)) {
          /* and call the old signal. */
          s_OldActions[a].sa_handler(sig);
          break;
        }
        /*
         * We matched the signal, but didn't handle it, so we emulate
         * the default behavior which is to exit the app with the signal
         * number as the error code.
         */
        NaClSignalErrorMessage("Failed to handle signal.\n");
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
  volatile struct ExceptionFrame *frame;
  uint32_t new_stack_ptr;
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
  new_stack_ptr -= sizeof(struct ExceptionFrame) - NACL_STACK_PAD_BELOW_ALIGN;
  new_stack_ptr = new_stack_ptr & ~NACL_STACK_ALIGN_MASK;
  new_stack_ptr -= NACL_STACK_PAD_BELOW_ALIGN;
  frame_addr = NaClUserToSysAddrRange(nap, new_stack_ptr,
                                      sizeof(struct ExceptionFrame));
  if (frame_addr == kNaClBadAddress) {
    /* We cannot write the stack frame. */
    return 0;
  }
  frame = (struct ExceptionFrame *) frame_addr;

#if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86
  frame->return_addr = kReturnAddr;
  regs->flags &= ~NACL_X86_DIRECTION_FLAG;
#endif

#if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 && NACL_BUILD_SUBARCH == 32
  frame->prog_ctr = regs->prog_ctr; /* Argument 1 */
  frame->stack_ptr = regs->stack_ptr; /* Argument 2 */

  regs->prog_ctr = nap->exception_handler;
  regs->stack_ptr = new_stack_ptr;
#elif NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 && NACL_BUILD_SUBARCH == 64
  /* Truncate the saved %rsp and %rbp values for portability. */
  regs->rdi = (uint32_t) regs->prog_ctr; /* Argument 1 */
  regs->rsi = (uint32_t) regs->stack_ptr; /* Argument 2 */

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
  /*
   * We write to memory here to get consistent behaviour for
   * unwritable stacks across architectures, to simplify testing.
   * TODO(mseaborn): This will eventually be replaced with writing a
   * register dump to the stack frame.
   */
  frame->dummy = 0;

  regs->lr = kReturnAddr;

  regs->r0 = regs->prog_ctr; /* Argument 1 */
  regs->r1 = regs->stack_ptr; /* Argument 2 */

  regs->prog_ctr = NaClUserToSys(nap, nap->exception_handler);
  regs->stack_ptr = NaClUserToSys(nap, new_stack_ptr);
#else
# error Unsupported architecture
#endif

  return 1;
}

static void SignalCatch(int sig, siginfo_t *info, void *uc) {
  struct NaClSignalContext sigCtx;
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

  NaClSignalContextFromHandler(&sigCtx, uc);
  NaClSignalContextGetCurrentThread(&sigCtx, &is_untrusted, &natp);

#if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 && NACL_BUILD_SUBARCH == 32
  if (is_untrusted) {
    /*
     * On Linux, the kernel does not restore %gs when entering the
     * signal handler, so we must do that here.  We need to do this
     * for glibc's TLS to work.  Some builds of glibc fetch a syscall
     * function pointer from glibc's static TLS area.  If we failed to
     * restore %gs, this function pointer would be under the control
     * of untrusted code, and we would have a vulnerability.
     *
     * Note that, in comparison, Breakpad tries to avoid using libc
     * calls at all when a crash occurs.
     *
     * On Mac OS X, the kernel *does* restore the original %gs when
     * entering the signal handler.  Our assignment to %gs here is
     * therefore not strictly necessary, but not harmful.  However,
     * this does mean we need to check the original %gs value (from
     * the signal frame) rather than the current %gs value (from
     * NaClGetGs()).
     *
     * Both systems necessarily restore %cs, %ds, and %ss otherwise
     * we would have a hard time handling signals in untrusted code
     * at all.
     */
    NaClSetGs(natp->sys.gs);
  }
#endif

  if (is_untrusted && sig == SIGSEGV) {
    if (DispatchToUntrustedHandler(natp, &sigCtx)) {
      NaClSignalContextToHandler(uc, &sigCtx);
      /* Resume untrusted code using the modified register state. */
      return;
    }
  }

  FindAndRunHandler(sig, info, uc);
}


void NaClSignalHandlerInitPlatform() {
  struct sigaction sa;
  int a;

  memset(&sa, 0, sizeof(sa));
  sigemptyset(&sa.sa_mask);
  sa.sa_sigaction = SignalCatch;
  sa.sa_flags = SA_ONSTACK | SA_SIGINFO;

  /* Mask all exceptions we catch to prevent re-entry */
  for (a = 0; a < SIGNAL_COUNT; a++) {
    sigaddset(&sa.sa_mask, s_Signals[a]);
  }

  /* Install all handlers */
  for (a = 0; a < SIGNAL_COUNT; a++) {
    if (sigaction(s_Signals[a], &sa, &s_OldActions[a]) != 0) {
      NaClLog(LOG_FATAL, "Failed to install handler for %d.\n\tERR:%s\n",
                          s_Signals[a], strerror(errno));
    }
  }
}

void NaClSignalHandlerFiniPlatform() {
  int a;

  /* Remove all handlers */
  for (a = 0; a < SIGNAL_COUNT; a++) {
    if (sigaction(s_Signals[a], &s_OldActions[a], NULL) != 0) {
      NaClLog(LOG_FATAL, "Failed to unregister handler for %d.\n\tERR:%s\n",
                          s_Signals[a], strerror(errno));
    }
  }
}

/*
 * Check that signal handlers are not registered.  We want to
 * discourage Chrome or libraries from registering signal handlers
 * themselves, because those signal handlers are often not safe when
 * triggered from untrusted code.  For background, see:
 * http://code.google.com/p/nativeclient/issues/detail?id=1607
 */
void NaClSignalAssertNoHandlers() {
  int index;
  for (index = 0; index < SIGNAL_COUNT; index++) {
    int signum = s_Signals[index];
    struct sigaction sa;
    if (sigaction(signum, NULL, &sa) != 0) {
      NaClLog(LOG_FATAL, "NaClSignalAssertNoHandlers: "
              "sigaction() call failed\n");
    }
    if ((sa.sa_flags & SA_SIGINFO) != 0
        ? sa.sa_sigaction != NULL
        : (sa.sa_handler != SIG_DFL && sa.sa_handler != SIG_IGN)) {
      NaClLog(LOG_FATAL, "NaClSignalAssertNoHandlers: "
              "A signal handler is registered for signal %d.  "
              "Did Breakpad register this?\n", signum);
    }
  }
}

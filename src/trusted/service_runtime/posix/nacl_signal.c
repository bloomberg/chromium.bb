/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <ucontext.h>

#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/service_runtime/nacl_config.h"
#include "native_client/src/trusted/service_runtime/nacl_globals.h"
#include "native_client/src/trusted/service_runtime/nacl_signal.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/service_runtime/sel_rt.h"

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

/*
 * The ucontext structure is common among POSIX implementations
 * but it's member mcontext is opaque.  The macros below map
 * a register index into this opaque structure.
 */
#if NACL_OSX
/* From: /usr/include/mach/i386/_structs.h */
  #define REG_EIP 10
  #define REG_RIP 16
  #if __DARWIN_UNIX03
    #define GET_CTX_REG(x, r) ((intptr_t *) &((x)->__ss))[r]
  #else
    #define GET_CTX_REG(x, r) ((intptr_t *) &((x)->ss))[r]
  #endif
#elif NACL_LINUX
/* From: /usr/include/sys/ucontext.h */
  #define GET_CTX_REG(x, r) (x).gregs[r]
#else
  #error Unknown platform!
#endif


intptr_t NaClGetIPFromUContext(ucontext_t *ctx) {
#if (NACL_ARCH(NACL_BUILD_ARCH) == NACL_arm) && NACL_LINUX
  return ctx->uc_mcontext.arm_pc;
#elif (NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86)
  #if NACL_TARGET_SUBARCH==32
    return GET_CTX_REG(ctx->uc_mcontext, REG_EIP);
  #elif NACL_TARGET_SUBARCH==64
    return GET_CTX_REG(ctx->uc_mcontext, REG_RIP);
  #endif
#else
  #error Unknown platform!
#endif
}

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
    NaClLog(LOG_FATAL, "Failed to mprotect() the stack guard page\n");
  }
  *result = stack;
  return 1;
}

void NaClSignalStackFree(void *stack) {
  CHECK(stack != NULL);
  if (munmap(stack, SIGNAL_STACK_SIZE + STACK_GUARD_SIZE) != 0) {
    NaClLog(LOG_FATAL, "Failed to munmap() signal stack\n");
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
    NaClLog(LOG_FATAL, "Failed to register signal stack\n");
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
  st.ss_size = 0;
  st.ss_sp = NULL;
  st.ss_flags = SS_DISABLE;
  if (sigaltstack(&st, NULL) != 0) {
    NaClLog(LOG_FATAL, "Failed to unregister signal stack\n");
  }
}


static void ExceptionCatch(int sig, siginfo_t *info, void *uc) {
  int untrusted = 0;

#if (NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86) && (NACL_TARGET_SUBARCH==32)
  struct NaClThreadContext *nacl_thread;
  uint16_t cs = NaClGetCs();
  uint16_t gs = NaClGetGs();
  uint16_t es = NaClGetEs();
  uint16_t fs = NaClGetFs();
  if (cs != NaClGetGlobalCs()) {
    /*
     * We need to restore segment registers before we can make any
     * calls, because some libraries will require them.
     */
    nacl_thread = nacl_sys[gs >> 3];
    NaClSetGs(nacl_thread->gs);
    NaClSetEs(nacl_thread->es);
    NaClSetFs(nacl_thread->es);
    untrusted = 1;
  }
#else
  uintptr_t ip = NaClGetIPFromUContext((ucontext_t *) uc);
  untrusted = g_SignalNAP && NaClIsUserAddr(g_SignalNAP, ip);
#endif

  if (NaClSignalHandlerFind(untrusted, sig, uc) == NACL_SIGNAL_SEARCH) {
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
         * We matched the signal, but didn't handle it, so we emualte
         * the default behavior which is to exit the app with the signal
         * number as the error code.
         */
        NaClSignalErrorMessage("Failed to handle signal.\n");
        _exit(-sig);
      }
    }
  }

#if (NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86) && (NACL_TARGET_SUBARCH==32)
  NaClSetGs(gs);
  NaClSetEs(es);
  NaClSetFs(fs);
#endif
}

void NaClSignalHandlerInitPlatform() {
  struct sigaction sa;
  int a;

  memset(&sa, 0, sizeof(sa));
  sigemptyset(&sa.sa_mask);
  sa.sa_sigaction = ExceptionCatch;
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

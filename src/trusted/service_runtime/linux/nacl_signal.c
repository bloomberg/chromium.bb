/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <signal.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/service_runtime/nacl_config.h"
#include "native_client/src/trusted/service_runtime/nacl_globals.h"
#include "native_client/src/trusted/service_runtime/nacl_signal.h"
#include "native_client/src/trusted/service_runtime/linux/nacl_signal_arch.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/service_runtime/sel_rt.h"


static const int kSignalStackSize = 8192; /* This is what Breakpad uses. */
static const int kStackGuardSize = NACL_PAGESIZE;

static struct NaClApp *g_nap;


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
  uint8_t *stack = mmap(NULL, kSignalStackSize + kStackGuardSize,
                        PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS,
                        -1, 0);
  if (stack == MAP_FAILED) {
    return 0;
  }
  /* We assume that the stack grows downwards. */
  if (mprotect(stack, kStackGuardSize, PROT_NONE) != 0) {
    NaClLog(LOG_FATAL, "Failed to mprotect() the stack guard page\n");
  }
  *result = stack;
  return 1;
}

void NaClSignalStackFree(void *stack) {
  CHECK(stack != NULL);
  if (munmap(stack, kSignalStackSize + kStackGuardSize) != 0) {
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
  st.ss_size = kSignalStackSize;
  st.ss_sp = ((uint8_t *) stack) + kStackGuardSize;
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

static void DebugMsg(const char *msg) {
  /*
   * We cannot use NaClLog() in the context of a signal handler: it is
   * too complex.  However, write() is signal-safe.
   */
  int len = strlen(msg);
  int rc = write(2, msg, len);
  if (rc != len) {
    /*
     * Newer glibc/gcc versions require that we check (because of
     * warn_unused_result + -Werror) but there's not much we can do
     * about a write() failure here.
     */
  }
}

static void NaClHandleFault(int in_untrusted_code) {
  if (in_untrusted_code) {
    DebugMsg("** Fault in NaCl untrusted code\n");
  }
  else {
    DebugMsg("** Fault in NaCl trusted code\n");

    /* TODO(mseaborn): Call Breakpad's crash reporter. */
    /* BreakpadSignalHandler(sig, info, uc); */
  }
}

void NaClSignalHandler(int sig, siginfo_t *info, void *uc) {
  ucontext_t *context = (ucontext_t *) uc;
  int in_untrusted_code;
  UNREFERENCED_PARAMETER(sig);
  UNREFERENCED_PARAMETER(info);

  /*
   * Sanity check: Make sure the signal stack frame was not allocated
   * in untrusted memory.  This checks that the alternate signal stack
   * is correctly set up, because otherwise, if it is not set up, the
   * test case would not detect that.
   *
   * There is little point in doing a CHECK instead of a DCHECK,
   * because if we are running off an untrusted stack, we have already
   * lost.
   */
  DCHECK(g_nap == NULL || !NaClIsUserAddr(g_nap, (uintptr_t) context));

  NaClRecoverFromSignal(g_nap, context, &in_untrusted_code);
  NaClHandleFault(in_untrusted_code);

  NaClSignalExit();
}


void NaClSignalHandlerInit() {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sigemptyset(&sa.sa_mask);
  sa.sa_sigaction = NaClSignalHandler;
  sa.sa_flags = SA_ONSTACK | SA_SIGINFO;
  if (sigaction(SIGSEGV, &sa, NULL) != 0) {
    NaClLog(LOG_FATAL, "Failed to register signal handler with sigaction()\n");
  }

  if (getenv("NACL_CRASH_TEST") != NULL) {
    DebugMsg("Causing crash in NaCl trusted code...\n");
    *(int *) 0 = 0;
  }
}


void NaClSignalHandlerFini() {
  /* TODO(mseaborn): Handle other signals too, such as SIGILL. */
  if (signal(SIGSEGV, SIG_DFL) == SIG_ERR) {
    NaClLog(LOG_FATAL, "Failed to unregister signal handler with signal()\n");
  }
}


void NaClSignalRegisterApp(struct NaClApp *nap) {
  CHECK(g_nap == NULL);
  g_nap = nap;
}

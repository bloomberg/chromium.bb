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
#include "native_client/src/trusted/service_runtime/sel_rt.h"


static const int kSignalStackSize = 8192; /* This is what Breakpad uses. */
static const int kStackGuardSize = NACL_PAGESIZE;

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

/* TODO(mseaborn): This is x86-32-only.  Support x86-64 and ARM. */
static void NaClSignalHandler(int sig, siginfo_t *info, void *uc) {
  ucontext_t *context = (ucontext_t *) uc;
  /*
   * We need to drop the top 16 bits with this implicit cast.  In some
   * situations, Linux does not assign to the top 2 bytes of the
   * REG_CS array entry when writing %cs to the stack.  Therefore we
   * need to drop the undefined top 2 bytes.  This happens in 32-bit
   * processes running on 64-bit kernels, but not on 32-bit kernels.
   */
  uint16_t cs = context->uc_mcontext.gregs[REG_CS];
  UNREFERENCED_PARAMETER(sig);
  UNREFERENCED_PARAMETER(info);

  if (cs != NaClGetGlobalCs()) {
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
    DebugMsg("** Fault in NaCl untrusted code\n");
  }
  else {
    DebugMsg("** Fault in NaCl trusted code\n");

    /* TODO(mseaborn): Call Breakpad's crash reporter. */
    /* BreakpadSignalHandler(sig, info, uc); */
  }

  /*
   * This is a sure fire, simple way of ensuring the process dies.
   * This assumes we're handling a SIGSEGV, in which case SIGSEGV is
   * blocked and the signal handler won't be re-entered.
   */
  __asm__("hlt");
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

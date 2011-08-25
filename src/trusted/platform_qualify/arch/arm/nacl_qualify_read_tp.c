/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <setjmp.h>
#include <stdlib.h>
#include <signal.h>

#include "native_client/src/trusted/platform_qualify/arch/arm/nacl_arm_qualify.h"

#if !NACL_LINUX
#error "A non-Linux ARM port?"
#endif

static sigjmp_buf try_state;

static void signal_catch(int sig) {
  siglongjmp(try_state, sig);
}

/*
 * Returns 1 if __aeabi_read_tp safely avoids reference to the stack.
 */
int NaClQualifyReadTp(void) {
  struct sigaction old_sigaction;
  struct sigaction try_sigaction;
  struct sigaltstack old_sigaltstack;
  struct sigaltstack try_sigaltstack;
  char signal_stack[SIGSTKSZ] __attribute__((aligned(16)));
  int success;

  try_sigaltstack.ss_sp = signal_stack;
  try_sigaltstack.ss_size = sizeof(signal_stack);
  try_sigaltstack.ss_flags = 0;
  if (0 != sigaltstack(&try_sigaltstack, &old_sigaltstack)) {
    return 0;
  }

  try_sigaction.sa_handler = signal_catch;
  sigemptyset(&try_sigaction.sa_mask);
  try_sigaction.sa_flags = 0;

  (void) sigaction(SIGSEGV, &try_sigaction, &old_sigaction);

  if (0 == sigsetjmp(try_state, 1)) {
    asm volatile(
        /*
         * Save the real stack pointer and then make the stack invalid
         * while we call __aeabi_read_tp.  If it uses the stack, we'll crash.
         */
        "mov r1, sp\n"
        "mov sp, %0\n"
        /*
         * Fetch the $tp value.
         * The ABI says this may not clobber any registers but r0.
         */
        "bl __aeabi_read_tp\n"
        /*
         * Now restore the real stack pointer.
         */
        "mov sp, r1\n"
        : : "r"(0) : "r0", "r1");
    success = 1;
  } else {
    success = 0;
  }

  (void) sigaction(SIGSEGV, &old_sigaction, NULL);
  (void) sigaltstack(&old_sigaltstack, NULL);

  return success;
}

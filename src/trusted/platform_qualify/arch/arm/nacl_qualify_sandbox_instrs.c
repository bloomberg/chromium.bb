/*
 * Copyright (c) 2013 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <setjmp.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>

#include "native_client/src/include/build_config.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/arm_sandbox.h"
#include "native_client/src/trusted/platform_qualify/arch/arm/nacl_arm_qualify.h"

static sigjmp_buf try_state;

static void signal_catch(int sig) {
  siglongjmp(try_state, sig);
}

/*
 * Returns 1 if special sandbox instructions trap as expected.
 */
int NaClQualifySandboxInstrs(void) {
  struct sigaction old_sigaction_trap;
  struct sigaction old_sigaction_ill;
#if NACL_ANDROID
  struct sigaction old_sigaction_bus;
#endif
  struct sigaction try_sigaction;
  volatile int fell_through = 0;

  try_sigaction.sa_handler = signal_catch;
  sigemptyset(&try_sigaction.sa_mask);
  try_sigaction.sa_flags = 0;

  (void) sigaction(SIGTRAP, &try_sigaction, &old_sigaction_trap);
  (void) sigaction(SIGILL, &try_sigaction, &old_sigaction_ill);
#if NACL_ANDROID
  /*
   * Android yields a SIGBUS for the breakpoint instruction used to mark
   * literal pools heads.
   */
  (void) sigaction(SIGBUS, &try_sigaction, &old_sigaction_bus);
#endif

  /* Each of the following should trap, successively executing
     each else clause, never the fallthrough. */
  if (0 == sigsetjmp(try_state, 1)) {
    asm(".word " NACL_TO_STRING(NACL_INSTR_ARM_LITERAL_POOL_HEAD) "\n");
    fell_through = 1;
  } else if (0 == sigsetjmp(try_state, 1)) {
    asm(".word " NACL_TO_STRING(NACL_INSTR_ARM_BREAKPOINT) "\n");
    fell_through = 1;
  } else if (0 == sigsetjmp(try_state, 1)) {
    asm(".word " NACL_TO_STRING(NACL_INSTR_ARM_HALT_FILL) "\n");
    fell_through = 1;
  } else if (0 == sigsetjmp(try_state, 1)) {
    asm(".word " NACL_TO_STRING(NACL_INSTR_ARM_ABORT_NOW) "\n");
    fell_through = 1;
  } else if (0 == sigsetjmp(try_state, 1)) {
    asm(".word " NACL_TO_STRING(NACL_INSTR_ARM_FAIL_VALIDATION) "\n");
    fell_through = 1;
  }

#if NACL_ANDROID
  (void) sigaction(SIGBUS, &old_sigaction_bus, NULL);
#endif
  (void) sigaction(SIGILL, &old_sigaction_ill, NULL);
  (void) sigaction(SIGTRAP, &old_sigaction_trap, NULL);

  return !fell_through;
}

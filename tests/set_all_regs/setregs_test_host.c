/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/shared/gio/gio.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_exit.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/service_runtime/include/bits/nacl_syscalls.h"
#include "native_client/src/trusted/service_runtime/nacl_all_modules.h"
#include "native_client/src/trusted/service_runtime/nacl_app.h"
#include "native_client/src/trusted/service_runtime/nacl_app_thread.h"
#include "native_client/src/trusted/service_runtime/nacl_copy.h"
#include "native_client/src/trusted/service_runtime/nacl_switch_to_app.h"
#include "native_client/src/trusted/service_runtime/nacl_syscall_common.h"
#include "native_client/src/trusted/service_runtime/nacl_valgrind_hooks.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"


/*
 * This implements a NaCl syscall that returns to untrusted code with
 * all registers restored from the given NaClSignalContext argument.
 *
 * This is for testing only, and so it does not attempt to be secure.
 * In particular, it does not ensure that the prog_ctr field is
 * bundle-aligned.
 */
int32_t SetAllRegsSyscall(struct NaClAppThread *natp) {
  uint32_t reg_state_uptr;
  struct NaClSignalContext reg_state;

  NaClCopyInDropLock(natp->nap);
  CHECK(NaClCopyInFromUser(natp->nap, &reg_state_uptr, natp->usr_syscall_args,
                           sizeof(reg_state_uptr)));
  CHECK(NaClCopyInFromUser(natp->nap, &reg_state, reg_state_uptr,
                           sizeof(reg_state)));

#if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86 && NACL_BUILD_SUBARCH == 64
  /* Apply x86-64 sandbox constraints. */
  reg_state.r15 = natp->user.r15;
  reg_state.prog_ctr = natp->user.r15 + (uint32_t) reg_state.prog_ctr;
  reg_state.stack_ptr = natp->user.r15 + (uint32_t) reg_state.stack_ptr;
  reg_state.rbp = natp->user.r15 + (uint32_t) reg_state.rbp;
#endif

  /* This duplicates some of the syscall exit path from nacl_syscall_hook.c. */
  NaClAppThreadSetSuspendState(natp, NACL_APP_THREAD_TRUSTED,
                               NACL_APP_THREAD_UNTRUSTED);

  NaClSwitchAllRegs(natp, &reg_state);
  /* Should not reach here. */
  return 0;
}

int main(int argc, char **argv) {
  struct NaClApp app;
  struct GioMemoryFileSnapshot gio_file;

  NaClHandleBootstrapArgs(&argc, &argv);

  NaClAllModulesInit();

  if (argc != 2) {
    NaClLog(LOG_FATAL, "Expected 1 argument: executable filename\n");
  }

  NaClAddSyscall(NACL_sys_test_syscall_1, SetAllRegsSyscall);

  NaClFileNameForValgrind(argv[1]);
  CHECK(GioMemoryFileSnapshotCtor(&gio_file, argv[1]));
  CHECK(NaClAppCtor(&app));
  CHECK(NaClAppLoadFile((struct Gio *) &gio_file, &app) == LOAD_OK);
  NaClAppInitialDescriptorHookup(&app);
  CHECK(NaClAppPrepareToLaunch(&app) == LOAD_OK);
  CHECK(NaClCreateMainThread(&app, 0, NULL, NULL));
  CHECK(NaClWaitForMainThreadToExit(&app) == 0);

  /*
   * Avoid calling exit() because it runs process-global destructors
   * which might break code that is running in our unjoined threads.
   */
  NaClExit(0);
}

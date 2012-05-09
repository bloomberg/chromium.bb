/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * NaCl service run-time.
 */

#include "native_client/src/include/portability.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "native_client/src/shared/platform/nacl_exit.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/service_runtime/nacl_globals.h"
#include "native_client/src/trusted/service_runtime/nacl_config.h"
#include "native_client/src/trusted/service_runtime/nacl_copy.h"
#include "native_client/src/trusted/service_runtime/nacl_switch_to_app.h"
#include "native_client/src/trusted/service_runtime/nacl_syscall_handlers.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/service_runtime/sel_rt.h"

#include "native_client/src/trusted/service_runtime/include/sys/errno.h"
#include "native_client/src/trusted/service_runtime/include/bits/nacl_syscalls.h"
#include "native_client/src/trusted/service_runtime/nacl_app_thread.h"
#include "native_client/src/trusted/service_runtime/nacl_stack_safety.h"


NORETURN void NaClSyscallCSegHook(int32_t tls_idx) {
  struct NaClAppThread      *natp;
  struct NaClApp            *nap;
  struct NaClThreadContext  *user;
  uintptr_t                 tramp_ret;
  nacl_reg_t                user_ret;
  size_t                    sysnum;
  uintptr_t                 sp_user;
  uintptr_t                 sp_sys;

  /*
   * Mark the thread as running on a trusted stack as soon as possible
   * so that we can report any crashes that occur after this point.
   */
  NaClStackSafetyNowOnTrustedStack();

  natp = nacl_thread[tls_idx];

  /*
   * Before this call, the thread could be suspended, so we should not
   * lock any mutexes before this, otherwise it could cause a
   * deadlock.
   */
  NaClAppThreadSetSuspendState(natp, NACL_APP_THREAD_UNTRUSTED,
                               NACL_APP_THREAD_TRUSTED);

  nap = natp->nap;
  user = &natp->user;
  sp_user = NaClGetThreadCtxSp(user);

  /* sp must be okay for control to have gotten here */
#if !BENCHMARK
  NaClLog(4, "Entered NaClSyscallCSegHook\n");
  NaClLog(4, "user sp %"NACL_PRIxPTR"\n", sp_user);
#endif

  /*
   * on x86_32 user stack:
   *  esp+0:  retaddr from lcall
   *  esp+4:  code seg from lcall
   *  esp+8:  retaddr to user module
   *  esp+c:  ...
   *
   * on ARM user stack
   *   sp+0:  retaddr from trampoline slot
   *   sp+4:  retaddr to user module
   *   sp+8:  arg0 to system call
   *   sp+c:  arg1 to system call
   *   sp+10: ....
   */

  sp_sys = NaClUserToSysStackAddr(nap, sp_user);
  /*
   * sp_sys points to the top of user stack where there is a retaddr to
   * trampoline slot.  NB: on x86-64, NaCl*StackAddr does no range check.
   */

  NaClCopyInTakeLock(nap);
  /*
   * held until syscall args are copied, which occurs in the generated
   * code.
   */

  tramp_ret = *(uintptr_t *) sp_sys;
  tramp_ret = NaClUserToSysStackAddr(nap, tramp_ret);

  sysnum = (tramp_ret - (nap->mem_start + NACL_SYSCALL_START_ADDR))
      >> NACL_SYSCALL_BLOCK_SHIFT;

#if !BENCHMARK
  NaClLog(4, "system call %"NACL_PRIuS"\n", sysnum);
#endif

  /*
   * getting user return address (the address where we need to return after
   * system call) from the user stack. (see stack layout above)
   */
  user_ret = *(uintptr_t *) (sp_sys + NACL_USERRET_FIX);
  /*
   * usr_syscall_args is used by Decoder functions in
   * nacl_syscall_handlers.c which is automatically generated file and
   * placed in the
   * scons-out/.../gen/native_client/src/trusted/service_runtime/
   * directory.  usr_syscall_args must point to the first argument of
   * a system call. System call arguments are placed on the untrusted
   * user stack.
   *
   * We save the user address for user syscall arguments fetching and
   * for VM range locking.
   */
  natp->usr_syscall_args = NaClRawUserStackAddrNormalize(sp_user +
                                                         NACL_SYSARGS_FIX);
  /*
   * Fix the user stack, throw away return addresses from the top of
   * the stack.
   */
  sp_user += NACL_SYSCALLRET_FIX;
  NaClSetThreadCtxSp(user, sp_user);

  if (sysnum >= NACL_MAX_SYSCALLS) {
    NaClLog(2, "INVALID system call %"NACL_PRIdS"\n", sysnum);
    natp->sysret = -NACL_ABI_EINVAL;
    NaClCopyInDropLock(nap);
  } else {
#if !BENCHMARK
    NaClLog(4, "making system call %"NACL_PRIdS", "
            "handler 0x%08"NACL_PRIxPTR"\n",
            sysnum, (uintptr_t) nap->syscall_table[sysnum].handler);
#endif
    natp->sysret = (*(nap->syscall_table[sysnum].handler))(natp);
    /* Implicitly drops lock */
  }
#if !BENCHMARK
  NaClLog(4,
          ("returning from system call %"NACL_PRIdS", return value %"NACL_PRId32
           " (0x%"NACL_PRIx32")\n"),
          sysnum, natp->sysret, natp->sysret);

  NaClLog(4, "return target 0x%08"NACL_PRIxNACL_REG"\n", user_ret);
  NaClLog(4, "user sp %"NACL_PRIxPTR"\n", sp_user);
#endif

  /*
   * before switching back to user module, we need to make sure that the
   * user_ret is properly sandboxed.
   */
  user_ret = (nacl_reg_t) NaClSandboxCodeAddr(nap, (uintptr_t) user_ret);
  /*
   * After this NaClAppThreadSetSuspendState() call, we should not
   * claim any mutexes, otherwise we risk deadlock.  Note that if
   * NACLVERBOSITY is set high enough to enable the NaClLog() calls in
   * NaClSwitchToApp(), these calls could deadlock on Windows.
   */
  NaClAppThreadSetSuspendState(natp, NACL_APP_THREAD_TRUSTED,
                               NACL_APP_THREAD_UNTRUSTED);
  NaClStackSafetyNowOnUntrustedStack();

  NaClSwitchToApp(natp, user_ret);
  /* NOTREACHED */

  fprintf(stderr, "NORETURN NaClSwitchToApp returned!?!\n");
  NaClAbort();
}

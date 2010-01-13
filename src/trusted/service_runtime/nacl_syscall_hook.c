/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * NaCl service run-time.
 */

#include "native_client/src/include/portability.h"

#include <stdlib.h>
#include <errno.h>
#include <time.h>

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/service_runtime/nacl_globals.h"
#include "native_client/src/trusted/service_runtime/nacl_config.h"
#include "native_client/src/trusted/service_runtime/nacl_switch_to_app.h"
#include "native_client/src/trusted/service_runtime/nacl_syscall_handlers.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/service_runtime/sel_rt.h"

#include "native_client/src/trusted/service_runtime/include/sys/errno.h"
#include "native_client/src/trusted/service_runtime/include/bits/nacl_syscalls.h"
#include "native_client/src/trusted/service_runtime/nacl_app_thread.h"
/*
 * Simple RPC support.  The default socket file descriptor is invalid.
 */
int NaClSrpcFileDescriptor = -1;

int NaClArtificialDelay = -1;


/*
 * The first syscall is from the NaCl module's main thread, and there
 * are no other user threads yet, so NACLDELAY check and the NACLCLOCK
 * usages are okay; when the NaCl module is multithreaded, the
 * variables they initialize are read-only.
 */


void NaClMicroSleep(int microseconds) {
  static int    initialized = 0;
  static tick_t cpu_clock = 0;
  tick_t        now;
  tick_t        end;

  if (!initialized) {
    char *env = getenv("NACLCLOCK");
    if (NULL != env) {
      cpu_clock = strtoul(env, (char **) NULL, 0);
    }

    initialized = 1;
  }

  now = get_ticks();
  end = now + (cpu_clock * microseconds) / 1000000;
  NaClLog(5, "Now %"PRId64".  Waiting until %"PRId64".\n", now, end);
  while (get_ticks() < end)
    ;
}


NORETURN void NaClSyscallCSegHook(int32_t tls_idx) {
  struct NaClAppThread      *natp = nacl_thread[tls_idx];
  struct NaClApp            *nap = natp->nap;
  struct NaClThreadContext  *user = &natp->user;
  uintptr_t                 tramp_ret;
  uintptr_t                 user_ret;
  uint32_t                  sysnum;
  uintptr_t                 sp_user;
  uintptr_t                 sp_sys;

  sp_user = NaClGetThreadCtxSp(user);

  /* sp must be okay for control to have gotten here */
#if !BENCHMARK
  NaClLog(4, "Entered NaClSyscallCSegHook\n");
  NaClLog(4, "user sp %"PRIxPTR"\n", sp_user);
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

  sp_sys = NaClUserToSys(nap, sp_user);
  /*
   * sp_sys points to the top of user stack where there is a retaddr to
   * trampoline slot
   */
  tramp_ret = *(uintptr_t *)sp_sys;
  tramp_ret = NaClUserToSys(nap, tramp_ret);

  sysnum = (tramp_ret - (nap->mem_start + NACL_SYSCALL_START_ADDR))
      >> NACL_SYSCALL_BLOCK_SHIFT;

#if !BENCHMARK
  NaClLog(4, "system call %d\n", sysnum);
#endif

  /*
   * getting user return address (the address where we need to return after
   * system call) from the user stack. (see stack layout above)
   */
  user_ret = *(uint32_t *) (sp_sys + NACL_USERRET_FIX);
  /*
   * Fix the user stack, throw away return addresses from the top of the stack.
   * After this fix, the first argument to a system call must be on the top of
   * the user stack (see user stack layout above)
   */
  sp_sys += NACL_SYSARGS_FIX;
  sp_user += NACL_SYSARGS_FIX;
  NaClSetThreadCtxSp(user, sp_user);

  if (sysnum >= NACL_MAX_SYSCALLS) {
    NaClLog(2, "INVALID system call %d\n", sysnum);
    natp->sysret = -NACL_ABI_EINVAL;
  } else {
#if !BENCHMARK
    NaClLog(4, "making system call %d, handler 0x%08"PRIxPTR"\n",
            sysnum, (uintptr_t) nacl_syscall[sysnum].handler);
#endif
    /*
     * x_sp is used by Decoder functions in nacl_syscall_handlers.c which is
     * automatically generated file and placed in
     * scons-out/.../gen/native_client/src/trusted/service_runtime. x_sp must
     * point to the first argument of a system call. System call arguments are
     * placed on the stack.
     */
    natp->x_sp = (uint32_t *) sp_sys;
    natp->sysret = (*nacl_syscall[sysnum].handler)(natp);
  }
#if !BENCHMARK
  NaClLog(4,
          ("returning from system call %d, return value %"PRId32
           " (0x%"PRIx32")\n"),
          sysnum, natp->sysret, natp->sysret);

  NaClLog(4, "return target 0x%08"PRIxPTR"\n", user_ret);
  NaClLog(4, "user sp %"PRIxPTR"\n", sp_user);
#endif
  if (-1 == NaClArtificialDelay) {
    char *delay = getenv("NACLDELAY");
    if (NULL != delay) {
      NaClArtificialDelay = strtol(delay, (char **) NULL, 0);
      NaClLog(0, "ARTIFICIAL DELAY %d us\n", NaClArtificialDelay);
    } else {
      NaClArtificialDelay = 0;
    }
  }
  if (0 != NaClArtificialDelay) {
    NaClMicroSleep(NaClArtificialDelay);
  }

  /*
   * before switching back to user module, we need to make sure that the
   * user_ret is properly sandboxed.
   */
  user_ret = NaClSandboxCodeAddr(nap, user_ret);

  NaClSwitchToApp(natp, user_ret);
 /* NOTREACHED */

  fprintf(stderr, "NORETURN NaClSwitchToApp returned!?!\n");
  abort();
}


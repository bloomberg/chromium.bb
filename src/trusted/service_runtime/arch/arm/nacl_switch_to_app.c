/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * NaCl Service Runtime, C-level context switch code.
 */

#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/service_runtime/arch/arm/sel_rt.h"
#include "native_client/src/trusted/service_runtime/nacl_app_thread.h"
#include "native_client/src/trusted/service_runtime/nacl_globals.h"
#include "native_client/src/trusted/service_runtime/nacl_switch_to_app.h"


NORETURN void NaClStartThreadInApp(struct NaClAppThread *natp,
                                   uint32_t             new_prog_ctr) {
  struct NaClApp  *nap;
  struct NaClThreadContext  *context;

  natp->sys.stack_ptr = (NaClGetSp() & ~0xf) + 4;

  /*
   * springboard pops 4 words from stack which are the parameters for
   * syscall. In this case, it is not a syscall so no parameters, but we still
   * need to adjust the stack
   */
  natp->user.stack_ptr -= 16;
  nap = natp->nap;
  context = &natp->user;
  context->spring_addr = NaClSysToUser(nap,
                                       nap->mem_start + nap->springboard_addr);
  context->new_eip = new_prog_ctr;
  context->sysret = 0; /* not used to return */

  NaClSwitch(context);
}

/*
 * syscall return
 */
NORETURN void NaClSwitchToApp(struct NaClAppThread *natp,
                              uint32_t             new_prog_ctr) {
  struct NaClApp  *nap;
  struct NaClThreadContext  *context;

  nap = natp->nap;
  context = &natp->user;
  context->spring_addr = NaClSysToUser(nap,
                                       nap->mem_start + nap->springboard_addr);
  context->new_eip = new_prog_ctr;
  context->sysret = natp->sysret;

  NaClSwitch(context);
}

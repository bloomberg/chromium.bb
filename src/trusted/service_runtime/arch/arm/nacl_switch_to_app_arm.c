/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
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

  natp->sys.stack_ptr = (NaClGetStackPtr() & ~0xf) + 4;

  nap = natp->nap;
  context = &natp->user;
  context->spring_addr = NaClSysToUser(nap,
                                       nap->mem_start + nap->springboard_addr);
  context->new_eip = new_prog_ctr;

  /*
   * At startup this is not the return value, but the first argument.
   * In the initial thread, it gets the pointer to the information
   * block on the stack.  Additional threads do not expect anything in
   * particular in the first argument register, so we don't bother to
   * conditionalize this.
   */
  context->sysret = context->stack_ptr;

  /*
   * springboard pops 4 words from stack which are the parameters for
   * syscall. In this case, it is not a syscall so no parameters, but we still
   * need to adjust the stack
   */
  context->stack_ptr -= 16;

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

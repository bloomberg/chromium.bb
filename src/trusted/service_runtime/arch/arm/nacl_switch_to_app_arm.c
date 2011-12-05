/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * NaCl Service Runtime, C-level context switch code.
 */

#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/service_runtime/arch/arm/sel_rt.h"
#include "native_client/src/trusted/service_runtime/nacl_app_thread.h"
#include "native_client/src/trusted/service_runtime/nacl_globals.h"
#include "native_client/src/trusted/service_runtime/nacl_switch_to_app.h"

void NaClInitSwitchToApp(struct NaClApp *nap) {
  /*
   * We don't need anything here.  We might need it in future if e.g.
   * we start letting untrusted code use NEON extensions.
   */
  UNREFERENCED_PARAMETER(nap);
}

/*
 * Does a quick calculation to get the real springboard address.
 * This is necessary because the springboard_addr is being used both as a code
 * and data pointer simultaneously which is a no go with thumb in the picture.
 *
 * An odd control transfer target address is how arm processors switch to
 * thumb mode.  If either the entry points (dynamic loader or user code) are
 * thumb mode, then we are in thumb mode.  and if we are in thumb mode, the
 * springboard target address must also be a thumb-mode address.
 */
static inline uintptr_t SpringboardAddr(struct NaClApp *nap) {
#if defined(NACL_TARGET_ARM_THUMB2_MODE)
  /*
   * An odd control flow transfer target address is how ARM processors switch
   * to thumb mode.  If either of the entry points (dynamic loader or user
   * code) is to thumb mode targets, then we are in thumb mode.  If we are in
   * thumb mode, the springboard target address must also be a thumb-mode
   * address.
   */
  CHECK((nap->user_entry_pt & 0x1) | (nap->initial_entry_pt & 0x1));
  /* The real springboard target addresses are aligned 0xe mod 16. */
  /* Skipping a 2-byte halt brings us to 0 mod 16. */
  CHECK((nap->springboard_addr & 0xf) == 0x0);
  return nap->springboard_addr | 0x1;
#else
  return nap->springboard_addr;
#endif
}

NORETURN void NaClStartThreadInApp(struct NaClAppThread *natp,
                                   uint32_t             new_prog_ctr) {
  struct NaClApp  *nap;
  struct NaClThreadContext  *context;

  natp->sys.stack_ptr = NaClGetStackPtr() & ~0xf;

  nap = natp->nap;
  context = &natp->user;
  context->spring_addr = NaClSysToUser(nap,
                                       nap->mem_start + SpringboardAddr(nap));
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
                                       nap->mem_start + SpringboardAddr(nap));
  context->new_eip = new_prog_ctr;
  context->sysret = natp->sysret;

  NaClSwitch(context);
}

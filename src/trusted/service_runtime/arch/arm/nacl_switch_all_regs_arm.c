/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/service_runtime/nacl_app_thread.h"
#include "native_client/src/trusted/service_runtime/nacl_signal.h"
#include "native_client/src/trusted/service_runtime/nacl_switch_to_app.h"


NORETURN void NaClSwitchAllRegsAsm(struct NaClSignalContext *state);

void NaClSwitchAllRegs(struct NaClAppThread *natp,
                       const struct NaClSignalContext *regs) {
  /* TODO(mseaborn): Copying the whole register set is suboptimal. */
  struct NaClSignalContext regs_copy = *regs;
  regs_copy.r9 = natp->user.r9;
  NaClSwitchAllRegsAsm(&regs_copy);
}

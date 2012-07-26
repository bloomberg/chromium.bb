/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/service_runtime/nacl_signal.h"
#include "native_client/src/trusted/service_runtime/nacl_switch_to_app.h"


/*
 * These are the fields that are popped off the stack by the 'iretq'
 * instruction.
 */
struct IretqData {
  uint64_t prog_ctr;
  uint64_t cs;
  uint64_t flags;
  uint64_t stack_ptr;
  uint64_t ss;
};

struct NaClSwitchAllRegsState {
  struct NaClSignalContext context;
  struct IretqData iret;
};


uint16_t NaClGetCs();
uint16_t NaClGetSs();
NORETURN void NaClSwitchAllRegsAsm(struct NaClSwitchAllRegsState *state);


void NaClSwitchAllRegs(struct NaClAppThread *natp,
                       struct NaClSignalContext *regs) {
  struct NaClSwitchAllRegsState state;

  UNREFERENCED_PARAMETER(natp);

  state.context = *regs;
  state.iret.prog_ctr = regs->prog_ctr;
  state.iret.cs = NaClGetCs();
  state.iret.flags = regs->flags;
  state.iret.stack_ptr = regs->stack_ptr;
  state.iret.ss = NaClGetSs();

  NaClSwitchAllRegsAsm(&state);
}

/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/service_runtime/nacl_app_thread.h"
#include "native_client/src/trusted/service_runtime/nacl_signal.h"
#include "native_client/src/trusted/service_runtime/nacl_switch_to_app.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"


struct NaClSwitchAllRegsState {
  /* These are adjacent because they are restored using 'lss'. */
  /* 0x00 */ uint32_t stack_ptr;
  /* 0x04 */ uint32_t ss;

  /* These are adjacent because they are restored using 'ljmp'. */
  /* 0x08 */ uint32_t spring_addr;
  /* 0x0c */ uint32_t cs;

  /* 0x10 */ uint16_t ds;
  /* 0x12 */ uint16_t es;
  /* 0x14 */ uint16_t fs;
  /* 0x16 */ uint16_t gs;

  /*
   * %ecx is missing from this list because it is restored from the %gs
   * segment as a last step.
   */
  /* 0x18 */ uint32_t eax;
  /* 0x1c */ uint32_t edx;
  /* 0x20 */ uint32_t ebx;
  /* 0x24 */ uint32_t ebp;
  /* 0x28 */ uint32_t esi;
  /* 0x2c */ uint32_t edi;

  /* 0x30 */ uint32_t flags;
};

NORETURN void NaClSwitchAllRegsAsm(struct NaClSwitchAllRegsState *state);

void NaClSwitchAllRegs(struct NaClAppThread *natp,
                       struct NaClSignalContext *regs) {
  struct NaClSwitchAllRegsState state;

  natp->tls_values.new_prog_ctr = regs->prog_ctr;
  natp->tls_values.new_ecx = regs->ecx;

  state.stack_ptr = regs->stack_ptr;
  state.ss = natp->user.ss;
  state.spring_addr = natp->nap->springboard_all_regs_addr;
  state.cs = natp->user.cs;
  state.ds = natp->user.ds;
  state.es = natp->user.es;
  state.fs = natp->user.fs;
  state.gs = natp->user.gs;
  state.eax = regs->eax;
  state.edx = regs->edx;
  state.ebx = regs->ebx;
  state.ebp = regs->ebp;
  state.esi = regs->esi;
  state.edi = regs->edi;
  state.flags = regs->flags;

  NaClSwitchAllRegsAsm(&state);
}

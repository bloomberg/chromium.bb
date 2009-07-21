/*
 * Copyright 2009, Google Inc.
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

#include <stdio.h>
#include <string.h>
#include "native_client/src/trusted/service_runtime/nacl_assert.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/service_runtime/arch/x86/sel_rt.h"
#include "native_client/src/trusted/service_runtime/tramp.h"
#include "native_client/src/trusted/service_runtime/nacl_globals.h"
#include "native_client/src/trusted/service_runtime/nacl_syscall_asm_symbols.h"


/*
 * A sanity check -- should be invoked in some early function, e.g.,
 * main, or something that main invokes early.
 */
void NaClThreadStartupCheck() {
  ASSERT(sizeof(struct NaClThreadContext) == 36);
}


/*
 * Install a syscall trampoline at target_addr.  NB: Thread-safe.
 */
void  NaClPatchOneTrampoline(struct NaClApp *nap,
                             uintptr_t  target_addr) {
  struct NaClPatchInfo  patch_info;

  struct NaClPatch      patch16[1];
  struct NaClPatch      patch32[2];

  patch16[0].target = ((uintptr_t) &NaCl_tramp_cseg_patch) - 2;
  patch16[0].value = nacl_global_cs;

  patch_info.abs16 = patch16;
  patch_info.num_abs16 = sizeof patch16/sizeof patch16[0];

  patch_info.rel32 = 0;
  patch_info.num_rel32 = 0;

  patch32[0].target = ((uintptr_t) &NaCl_tramp_cseg_patch) - 6;
  patch32[0].value = (uintptr_t) NaClSyscallSeg;

  patch32[1].target = ((uintptr_t) &NaCl_tramp_dseg_patch) - 4;
  patch32[1].value = nacl_global_ds;  /* opens the data sandbox */

  patch_info.abs32 = patch32;
  patch_info.num_abs32 = sizeof patch32/sizeof patch32[0];

  patch_info.dst = target_addr;
  patch_info.src = (uintptr_t) &NaCl_trampoline_seg_code;
  patch_info.nbytes = ((uintptr_t) &NaCl_trampoline_seg_end
                       - (uintptr_t) &NaCl_trampoline_seg_code);

  NaClApplyPatchToMemory(&patch_info);
}


void NaClFillTrampolineRegion(struct NaClApp *nap) {
  memset((void *) nap->mem_start, NACL_HALT_OPCODE, NACL_TRAMPOLINE_END);
}


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

#include "native_client/src/trusted/service_runtime/nacl_check.h"
#include "native_client/src/trusted/service_runtime/nacl_globals.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/service_runtime/springboard.h"
#include "native_client/src/trusted/service_runtime/tramp.h"
#include "native_client/src/trusted/service_runtime/arch/arm/sel_ldr_arm.h"

/*
 * Install a syscall trampoline at target_addr.  NB: Thread-safe.
 */
void  NaClPatchOneTrampoline(struct NaClApp *nap,
                             uintptr_t      target_addr) {
  struct NaClPatchInfo  patch_info;

  UNREFERENCED_PARAMETER(nap);
  /*
   * in ARM we do not need to patch ds, cs segments.
   * by default we initialize the target for trampoline code as NaClSyscallSeg,
   * so there is no point to patch address of NaClSyscallSeg
   */
  patch_info.num_abs16 = 0;
  patch_info.num_rel32 = 0;
  patch_info.num_abs32 = 0;

  patch_info.dst = target_addr;
  patch_info.src = (uintptr_t) &NaCl_trampoline_seg_code;
  patch_info.nbytes = ((uintptr_t) &NaCl_trampoline_seg_end
                       - (uintptr_t) &NaCl_trampoline_seg_code);

  NaClApplyPatchToMemory(&patch_info);
}


static void NaClFillMemoryRegionWithHalt(void *start, size_t size) {
  uint32_t *inst = (uint32_t *) start;
  uint32_t i;

  CHECK(0 == size % NACL_HALT_LEN);
  /* check that the region start is 4 bytes alligned */
  CHECK(0 == (uint32_t)start % NACL_HALT_LEN);

  for (i = 0; i < (size / NACL_HALT_LEN); i++)
    inst[i] = NACL_HALT_OPCODE;
}


void NaClFillTrampolineRegion(struct NaClApp *nap) {
  NaClFillMemoryRegionWithHalt((void *)(nap->mem_start + NACL_TRAMPOLINE_START),
                               NACL_TRAMPOLINE_SIZE);
}


/*
 * fill from text_region_bytes to end of that page with halt
 * instruction, which is one byte in size.
 */
void NaClFillEndOfTextRegion(struct NaClApp *nap) {
  size_t page_pad;

  page_pad = NaClRoundPage(nap->text_region_bytes) - nap->text_region_bytes;
  CHECK(page_pad < NACL_PAGESIZE);

  NaClFillMemoryRegionWithHalt((void *)(nap->mem_start +
                                        NACL_TRAMPOLINE_END +
                                        nap->text_region_bytes),
                               page_pad);

  nap->text_region_bytes += page_pad;
}

/*
 * patch a tls hook, which is used by nacl module for obtaining its tls pointer,
 * to the end of trampoline region minus one slot.
 */
void NaClLoadTlsHook(struct NaClApp *nap) {
  struct NaClPatchInfo  patch_info;
  struct NaClPatch      abs32;
  uintptr_t tls_hook_addr;

  NaClLog(2, "Installing tls hook\n");

  patch_info.rel32 = 0;
  patch_info.num_rel32 = 0;
  patch_info.abs32 = &abs32;
  patch_info.num_abs32 = 0;
  patch_info.abs16 = 0;
  patch_info.num_abs16 = 0;

  tls_hook_addr = NACL_TRAMPOLINE_END - 2*nap->align_boundary;

  patch_info.dst = nap->mem_start + tls_hook_addr;
  patch_info.src = (uintptr_t) &NaClReadTP_start;
  patch_info.nbytes = ((uintptr_t) &NaClReadTP_end
                       - (uintptr_t) &NaClReadTP_start);

  CHECK(patch_info.nbytes <= NACL_SYSCALL_BLOCK_SIZE);

  NaClApplyPatchToMemory(&patch_info);
}

void  NaClLoadSpringboard(struct NaClApp  *nap) {
  /*
   * patch in springboard.S code into space in place of
   * the last syscall in the trampoline region.
   */
  struct NaClPatchInfo  patch_info;
  struct NaClPatch      abs32;

  patch_info.rel32 = 0;
  patch_info.num_rel32 = 0;
  patch_info.abs32 = &abs32;
  patch_info.num_abs32 = 0;
  patch_info.abs16 = 0;
  patch_info.num_abs16 = 0;

  nap->springboard_addr = NACL_TRAMPOLINE_END - nap->align_boundary;

  patch_info.dst = nap->mem_start + nap->springboard_addr;
  patch_info.src = (uintptr_t) &NaCl_springboard;
  patch_info.nbytes = ((uintptr_t) &NaCl_springboard_end
                       - (uintptr_t) &NaCl_springboard);

  NaClApplyPatchToMemory(&patch_info);

  nap->springboard_addr += NACL_HALT_LEN; /* skip the hlt */
}

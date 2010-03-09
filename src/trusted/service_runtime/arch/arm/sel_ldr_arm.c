/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/trusted/service_runtime/nacl_globals.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/service_runtime/springboard.h"
#include "native_client/src/trusted/service_runtime/arch/arm/sel_ldr_arm.h"
#include "native_client/src/trusted/service_runtime/arch/arm/tramp_arm.h"


/* NOTE(robertm): the trampoline organization for ARM is currenly assuming
 * NACL_TRAMPOLINE_SIZE == 32. This is contrary to the bundle size
 * which is 16. The reason for this is tramp.S which has a payload
 * 5 instr + one data item
 */

/*
 * Install a syscall trampoline at target_addr.  NB: Thread-safe.
 * The code being patched is from tramp.S
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

  NaClPatchInfoCtor(&patch_info);

  patch_info.dst = target_addr;
  patch_info.src = (uintptr_t) &NaCl_trampoline_seg_code;
  patch_info.nbytes = ((uintptr_t) &NaCl_trampoline_seg_end
                       - (uintptr_t) &NaCl_trampoline_seg_code);

  NaClApplyPatchToMemory(&patch_info);
}


void NaClFillMemoryRegionWithHalt(void *start, size_t size) {
  uint32_t *inst = (uint32_t *) start;
  uint32_t i;

  CHECK(0 == size % NACL_HALT_LEN);
  /* check that the region start is 4 bytes aligned */
  CHECK(0 == (uint32_t)start % NACL_HALT_LEN);

  for (i = 0; i < (size / NACL_HALT_LEN); i++)
    inst[i] = NACL_HALT_OPCODE;
}


void NaClFillTrampolineRegion(struct NaClApp *nap) {
  NaClFillMemoryRegionWithHalt((void *)(nap->mem_start + NACL_TRAMPOLINE_START),
                               NACL_TRAMPOLINE_SIZE);
}


/*
 * Fill from static_text_end to end of that page with halt
 * instruction, which is NACL_HALT_LEN in size.  Does not touch
 * dynamic text region, which should be pre-filled with HLTs.
 */
void NaClFillEndOfTextRegion(struct NaClApp *nap) {
  size_t page_pad;

  page_pad = (NaClRoundAllocPage(nap->static_text_end + NACL_HALT_SLED_SIZE)
              - nap->static_text_end);
  CHECK(page_pad < NACL_MAP_PAGESIZE + NACL_HALT_SLED_SIZE);

  NaClLog(4,
          "Filling with halts: %08"NACL_PRIxPTR", %08"NACL_PRIxS" bytes\n",
          nap->mem_start + nap->static_text_end,
          page_pad);

  NaClFillMemoryRegionWithHalt((void *)(nap->mem_start + nap->static_text_end),
                               page_pad);

  nap->static_text_end += page_pad;
}

/*
 * patch in springboard.S code into space in place of
 * the last syscall in the trampoline region.
 * The code being patched is from springboard.S
 */

void  NaClLoadSpringboard(struct NaClApp  *nap) {
  struct NaClPatchInfo  patch_info;
  const uintptr_t       springboard_addr = NACL_TRAMPOLINE_END -
                                           NACL_SYSCALL_BLOCK_SIZE;
  NaClLog(2, "Installing springboard at 0x%08"NACL_PRIxPTR"\n",
          springboard_addr);

  NaClPatchInfoCtor(&patch_info);

  patch_info.dst = nap->mem_start + springboard_addr;
  patch_info.src = (uintptr_t) &NaCl_springboard;
  patch_info.nbytes = ((uintptr_t) &NaCl_springboard_end
                       - (uintptr_t) &NaCl_springboard);

  NaClApplyPatchToMemory(&patch_info);

  nap->springboard_addr = springboard_addr + NACL_HALT_LEN; /* skip the hlt */
}

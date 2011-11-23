/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
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
  struct NaClPatch      patch_syscall_seg;

  UNREFERENCED_PARAMETER(nap);

  /*
   * For ARM we only need to patch in the address of NaClSyscallSeg.
   * We only do even that in case we're PIC (to avoid a TEXTREL).
   */

  NaClPatchInfoCtor(&patch_info);

  patch_info.dst = target_addr;
  patch_info.src = (uintptr_t) &NaCl_trampoline_seg_code;
  patch_info.nbytes = ((uintptr_t) &NaCl_trampoline_seg_end
                       - (uintptr_t) &NaCl_trampoline_seg_code) - 4;

  patch_info.num_abs32 = 1;
  patch_info.abs32 = &patch_syscall_seg;
  patch_syscall_seg.target = (uintptr_t) &NaCl_trampoline_syscall_seg_addr;
  patch_syscall_seg.value = (uintptr_t) &NaClSyscallSeg;

  NaClApplyPatchToMemory(&patch_info);
}


void NaClFillMemoryRegionWithHalt(void *start, size_t size) {
#if defined(NACL_TARGET_ARM_THUMB2_MODE)
  uint16_t *inst = (uint16_t *) start;
#else
  uint32_t *inst = (uint32_t *) start;
#endif /* defined(NACL_TARGET_ARM_THUMB2_MODE) */
  uint32_t i;

  CHECK(sizeof *inst == NACL_HALT_LEN);
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
 * patch in springboard.S code into space in place of
 * the last syscall in the trampoline region.
 * The code being patched is from springboard.S
 */

void  NaClLoadSpringboard(struct NaClApp  *nap) {
  struct NaClPatchInfo  patch_info;
#if defined(NACL_TARGET_ARM_THUMB2_MODE)
  /* Springboard begins 2 bytes earlier for thumb2. */
  const uintptr_t       springboard_addr = NACL_TRAMPOLINE_END -
                                           NACL_SYSCALL_BLOCK_SIZE -
                                           NACL_HALT_LEN;
#else
  const uintptr_t       springboard_addr = NACL_TRAMPOLINE_END -
                                           NACL_SYSCALL_BLOCK_SIZE;
#endif  /* defined(NACL_TARGET_ARM_THUMB2_MODE) */
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

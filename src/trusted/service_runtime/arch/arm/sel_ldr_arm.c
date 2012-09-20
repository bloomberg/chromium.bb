/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/trusted/service_runtime/nacl_globals.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
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
void  NaClPatchOneTrampolineCall(uintptr_t call_target_addr,
                                 uintptr_t target_addr) {
  struct NaClPatchInfo patch_info;
  struct NaClPatch patch_syscall_seg;

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
  patch_syscall_seg.value = call_target_addr;

  NaClApplyPatchToMemory(&patch_info);
}

void  NaClPatchOneTrampoline(struct NaClApp *nap,
                             uintptr_t      target_addr) {
  UNREFERENCED_PARAMETER(nap);

  NaClPatchOneTrampolineCall((uintptr_t) &NaClSyscallSeg, target_addr);
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


void  NaClLoadSpringboard(struct NaClApp  *nap) {
  UNREFERENCED_PARAMETER(nap);
}

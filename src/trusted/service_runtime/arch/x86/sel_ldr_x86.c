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

#include "native_client/src/include/portability_string.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/trusted/service_runtime/nacl_check.h"
#include "native_client/src/trusted/service_runtime/nacl_syscall_asm_symbols.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/service_runtime/springboard.h"
#include "native_client/src/trusted/service_runtime/tramp.h"
#include "native_client/src/trusted/service_runtime/arch/x86/sel_ldr_x86.h"
#if NACL_BUILD_SUBARCH == 32
/* These files are generated during the build. */
#include "gen/native_client/src/trusted/service_runtime/arch/x86/tramp_data.h"
#include "gen/native_client/src/trusted/service_runtime/arch/x86/springboard_data.h"
#else
/* These files are checked in. */
#include "native_client/src/trusted/service_runtime/arch/x86_64/tramp_data.h"
#include "native_client/src/trusted/service_runtime/arch/x86_64/springboard_data.h"
#endif

/*
 * Install a syscall trampoline at target_addr.  NB: Thread-safe.
 */
void  NaClPatchOneTrampoline(struct NaClApp *nap,
                             uintptr_t      target_addr) {
  struct NaClPatchInfo  patch_info;

  struct NaClPatch      patch16[1];
  struct NaClPatch      patch32[2];

  UNREFERENCED_PARAMETER(nap);

  patch16[0].target = ((uintptr_t) kTrampolineCode) + NACL_TRAMP_CSEG_PATCH - 2;
  patch16[0].value = NaClGetGlobalCs();

  patch_info.abs16 = patch16;
  patch_info.num_abs16 = NACL_ARRAY_SIZE(patch16);

  patch_info.rel32 = NULL;
  patch_info.num_rel32 = 0;

  /* TODO: NaClSyscallSeg has to be in the lower 4 GB */
  patch32[0].target = ((uintptr_t) kTrampolineCode) + NACL_TRAMP_CSEG_PATCH - 6;
  patch32[0].value = (uintptr_t) NaClSyscallSeg;

  patch32[1].target = ((uintptr_t) kTrampolineCode) + NACL_TRAMP_DSEG_PATCH - 4;
  patch32[1].value = NaClGetGlobalDs();  /* opens the data sandbox */

  patch_info.abs32 = patch32;
  patch_info.num_abs32 = NACL_ARRAY_SIZE(patch32);

  patch_info.dst = target_addr;
  patch_info.src = (uintptr_t) kTrampolineCode;
  patch_info.nbytes = sizeof(kTrampolineCode);

  NaClApplyPatchToMemory(&patch_info);
}

static void NaClFillMemoryRegionWithHalt(void *start, size_t size) {
  CHECK(!(size % NACL_HALT_LEN));
  memset(start, NACL_HALT_OPCODE, size);
}

void NaClFillTrampolineRegion(struct NaClApp *nap) {
  NaClFillMemoryRegionWithHalt((void *) nap->mem_start, NACL_TRAMPOLINE_END);
}


/*
 * fill from text_region_bytes to end of that page with halt
 * instruction, which is one byte in size.
 */
void NaClFillEndOfTextRegion(struct NaClApp *nap) {
  size_t page_pad;

  page_pad = NaClRoundPage(nap->text_region_bytes) - nap->text_region_bytes;
  CHECK(page_pad < NACL_PAGESIZE);

  NaClFillMemoryRegionWithHalt((void *) (nap->mem_start +
                                         NACL_TRAMPOLINE_END +
                                         nap->text_region_bytes),
                                         page_pad);

  nap->text_region_bytes += page_pad;
}

void NaClLoadSpringboard(struct NaClApp  *nap) {
  /*
   * patch in springboard.S code into space in place of
   * the last syscall in the trampoline region.
   */
  struct NaClPatchInfo  patch_info;

  patch_info.rel32 = NULL;
  patch_info.num_rel32 = 0;
  patch_info.abs32 = NULL;
  patch_info.num_abs32 = 0;
  patch_info.abs16 = NULL;
  patch_info.num_abs16 = 0;

  nap->springboard_addr = NACL_TRAMPOLINE_END - nap->align_boundary;

  patch_info.dst = nap->mem_start + nap->springboard_addr;
  patch_info.src = (uintptr_t) kSpringboardCode;
  patch_info.nbytes = sizeof(kSpringboardCode);

  NaClApplyPatchToMemory(&patch_info);

  nap->springboard_addr += NACL_HALT_LEN; /* skip the hlt */
}


void NaClLoadTlsHook(struct NaClApp  *nap) {
  /*
   * x86 does not require TLS hook to be loaded in trampoline region, that is
   * why it must be empty.
   */
  UNREFERENCED_PARAMETER(nap);
}

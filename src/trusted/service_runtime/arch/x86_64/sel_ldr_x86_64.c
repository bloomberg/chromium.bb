/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/include/portability_string.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_platform.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/trusted/service_runtime/nacl_globals.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/service_runtime/sel_memory.h"
#include "native_client/src/trusted/service_runtime/springboard.h"
#include "native_client/src/trusted/service_runtime/arch/x86/sel_ldr_x86.h"
#include "native_client/src/trusted/service_runtime/arch/x86_64/sel_rt_64.h"
#include "native_client/src/trusted/service_runtime/arch/x86_64/tramp_64.h"

static uintptr_t AddDispatchThunk(uintptr_t *next_addr,
                                  uintptr_t target_routine) {
  struct NaClPatchInfo patch_info;
  struct NaClPatch jmp_target;

  jmp_target.target = (((uintptr_t) &NaClDispatchThunk_jmp_target)
                       - sizeof(uintptr_t));
  jmp_target.value = target_routine;

  NaClPatchInfoCtor(&patch_info);
  patch_info.abs64 = &jmp_target;
  patch_info.num_abs64 = 1;

  patch_info.dst = *next_addr;
  patch_info.src = (uintptr_t) &NaClDispatchThunk;
  patch_info.nbytes = ((uintptr_t) &NaClDispatchThunkEnd
                       - (uintptr_t) &NaClDispatchThunk);
  NaClApplyPatchToMemory(&patch_info);

  *next_addr += patch_info.nbytes;
  return patch_info.dst;
}

int NaClMakeDispatchThunk(struct NaClApp *nap) {
  int                   retval = 0;  /* fail */
  int                   error;
  void                  *thunk_addr = NULL;
  uintptr_t             next_addr;
  uintptr_t             dispatch_thunk = 0;
  uintptr_t             get_tls_fast_path1 = 0;
  uintptr_t             get_tls_fast_path2 = 0;

  NaClLog(2, "Entered NaClMakeDispatchThunk\n");
  if (0 != nap->dispatch_thunk) {
    NaClLog(LOG_ERROR, " dispatch_thunk already initialized!\n");
    return 1;
  }

  if (0 != (error = NaCl_page_alloc_randomized(&thunk_addr,
                                               NACL_MAP_PAGESIZE))) {
    NaClLog(LOG_INFO,
            "NaClMakeDispatchThunk::NaCl_page_alloc failed, errno %d\n",
            -error);
    retval = 0;
    goto cleanup;
  }
  NaClLog(2, "NaClMakeDispatchThunk: got addr 0x%"NACL_PRIxPTR"\n",
          (uintptr_t) thunk_addr);

  if (0 != (error = NaCl_mprotect(thunk_addr,
                                  NACL_MAP_PAGESIZE,
                                  PROT_READ | PROT_WRITE))) {
    NaClLog(LOG_INFO,
            "NaClMakeDispatchThunk::NaCl_mprotect r/w failed, errno %d\n",
            -error);
    retval = 0;
    goto cleanup;
  }
  NaClFillMemoryRegionWithHalt(thunk_addr, NACL_MAP_PAGESIZE);

  next_addr = (uintptr_t) thunk_addr;
  dispatch_thunk =
      AddDispatchThunk(&next_addr, (uintptr_t) &NaClSyscallSeg);
  get_tls_fast_path1 =
      AddDispatchThunk(&next_addr, (uintptr_t) &NaClGetTlsFastPath1);
  get_tls_fast_path2 =
      AddDispatchThunk(&next_addr, (uintptr_t) &NaClGetTlsFastPath2);

  if (0 != (error = NaCl_mprotect(thunk_addr,
                                  NACL_MAP_PAGESIZE,
                                  PROT_EXEC|PROT_READ))) {
    NaClLog(LOG_INFO,
            "NaClMakeDispatchThunk::NaCl_mprotect r/x failed, errno %d\n",
            -error);
    retval = 0;
    goto cleanup;
  }
  retval = 1;
 cleanup:
  if (0 == retval) {
    if (NULL != thunk_addr) {
      NaCl_page_free(thunk_addr, NACL_MAP_PAGESIZE);
      thunk_addr = NULL;
    }
  } else {
    nap->dispatch_thunk = dispatch_thunk;
    nap->get_tls_fast_path1 = get_tls_fast_path1;
    nap->get_tls_fast_path2 = get_tls_fast_path2;
  }
  return retval;
}

/*
 * Install a syscall trampoline at target_addr.  NB: Thread-safe.
 */
void  NaClPatchOneTrampolineCall(uintptr_t  call_target_addr,
                                 uintptr_t  target_addr) {
  struct NaClPatchInfo  patch_info;
  struct NaClPatch      call_target;

  NaClLog(6, "call_target_addr = 0x%"NACL_PRIxPTR"\n", call_target_addr);
  CHECK(0 != call_target_addr);
  call_target.target = (((uintptr_t) &NaCl_trampoline_call_target)
                        - sizeof(uintptr_t));
  call_target.value = call_target_addr;

  NaClPatchInfoCtor(&patch_info);

  patch_info.abs64 = &call_target;
  patch_info.num_abs64 = 1;

  patch_info.dst = target_addr;
  patch_info.src = (uintptr_t) &NaCl_trampoline_code;
  patch_info.nbytes = ((uintptr_t) &NaCl_trampoline_code_end
                       - (uintptr_t) &NaCl_trampoline_code);

  NaClApplyPatchToMemory(&patch_info);
}

void  NaClPatchOneTrampoline(struct NaClApp *nap,
                             uintptr_t      target_addr) {
  uintptr_t             call_target_addr;

  call_target_addr = nap->dispatch_thunk;
  NaClPatchOneTrampolineCall(call_target_addr, target_addr);
}

void NaClFillMemoryRegionWithHalt(void *start, size_t size) {
  CHECK(!(size % NACL_HALT_LEN));
  /* Tell valgrind that this memory is accessible and undefined */
  NACL_MAKE_MEM_UNDEFINED(start, size);
  memset(start, NACL_HALT_OPCODE, size);
}

void NaClFillTrampolineRegion(struct NaClApp *nap) {
  NaClFillMemoryRegionWithHalt(
      (void *) (nap->mem_start + NACL_TRAMPOLINE_START),
      NACL_TRAMPOLINE_SIZE);
}

void NaClLoadSpringboard(struct NaClApp  *nap) {
  /*
   * There is no springboard for x86-64.
   */
  UNREFERENCED_PARAMETER(nap);
}

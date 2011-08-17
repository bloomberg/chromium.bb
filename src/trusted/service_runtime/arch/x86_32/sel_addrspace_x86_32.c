/*
 * Copyright 2009 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/include/nacl_platform.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/trusted/service_runtime/sel_memory.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"


NaClErrorCode NaClAllocateSpace(void **mem, size_t addrsp_size) {
  int result;

  CHECK(NULL != mem);

#ifdef NACL_SANDBOX_FIXED_AT_ZERO
  /*
   * When creating a zero-based sandbox, we do not allocate the first 64K of
   * pages beneath the trampolines, because -- on Linux at least -- we cannot.
   * Instead, we allocate starting at the trampolines, and then coerce the
   * out parameter.
   */
  addrsp_size -= NACL_TRAMPOLINE_START;
  *mem = (void *) NACL_TRAMPOLINE_START;
  result = NaCl_page_alloc_at_addr(mem, addrsp_size);
  *mem = 0;
#elif NACL_WINDOWS && NACL_BUILD_SUBARCH == 32
  /*
   * On 32 bit Windows, a 1 gigabyte block of address space is reserved before
   * starting up this process to make sure we can create the sandbox. Look for
   * this pre-reserved block and if found, pass its address to the page
   * allocation function.
   */
  if (0 == NaCl_find_prereserved_sandbox_memory(mem, addrsp_size)) {
    result = NaCl_page_alloc_at_addr(mem, addrsp_size);
  } else {
    result = NaCl_page_alloc(mem, addrsp_size);
  }
#else
  result = NaCl_page_alloc(mem, addrsp_size);
#endif

  if (0 != result) {
    NaClLog(2,
        "NaClAllocateSpace: NaCl_page_alloc 0x%08"NACL_PRIxPTR
        " failed\n",
        (uintptr_t) *mem);
    return LOAD_NO_MEMORY;
  }
  NaClLog(4, "NaClAllocateSpace: %"NACL_PRIxPTR", %"NACL_PRIxS"\n",
          (uintptr_t) *mem,
          addrsp_size);

  return LOAD_OK;
}


NaClErrorCode NaClMprotectGuards(struct NaClApp *nap) {
  uintptr_t start_addr;
  int       err;

  start_addr = nap->mem_start;

  NaClLog(3,
          ("NULL detection region start 0x%08"NACL_PRIxPTR", "
           "size 0x%08x, end 0x%08"NACL_PRIxPTR"\n"),
          start_addr, NACL_SYSCALL_START_ADDR,
          start_addr + NACL_SYSCALL_START_ADDR);
  if ((err = NaCl_mprotect((void *) start_addr,
                           NACL_SYSCALL_START_ADDR,
                           PROT_NONE)) != 0) {
    NaClLog(LOG_ERROR, ("NaClMemoryProtection: "
                        "NaCl_mprotect(0x%08"NACL_PRIxPTR", "
                        "0x%08x, 0x%x) failed, "
                        "error %d (NULL pointer guard page)\n"),
            start_addr, NACL_SYSCALL_START_ADDR, PROT_NONE,
            err);
    return LOAD_MPROTECT_FAIL;
  }
  if (!NaClVmmapAdd(&nap->mem_map,
                    (start_addr - nap->mem_start) >> NACL_PAGESHIFT,
                    NACL_SYSCALL_START_ADDR >> NACL_PAGESHIFT,
                    PROT_NONE,
                    (struct NaClMemObj *) NULL)) {
    NaClLog(LOG_ERROR, ("NaClMemoryProtection: NaClVmmapAdd failed"
                        " (NULL pointer guard page)\n"));
    return LOAD_MPROTECT_FAIL;
  }

  return LOAD_OK;
}

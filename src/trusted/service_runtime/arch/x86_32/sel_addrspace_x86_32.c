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
  CHECK(NULL != mem);
  if (NaCl_page_alloc(mem, addrsp_size) != 0) {
    NaClLog(2, "NaClAlloccaterSpace: NaCl_page_alloc failed\n");
    return LOAD_NO_MEMORY;
  }
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

void NaClTeardownMprotectGuards(struct NaClApp *nap) {
  UNREFERENCED_PARAMETER(nap);
  return;
}

/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/include/nacl_platform.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/trusted/service_runtime/nacl_error_code.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/service_runtime/sel_memory.h"


/*
 * On ARM, we cheat slightly: we add two pages to the requested allocation!
 * This accomodates the guard region we require at the top end of untrusted
 * memory.
 */
int NaClAllocateSpace(void **mem, size_t size) {
  CHECK(mem);

  size += 2 * NACL_PAGESIZE;

  *mem = (void *) NACL_TRAMPOLINE_START;
  if (NaCl_page_alloc_at_addr(mem, size) != 0) {
    NaClLog(2,
        "NaClAlloccaterSpace: NaCl_page_alloc_at_addr 0x%08"PRIxPTR" failed\n",
        (uintptr_t) *mem);
    return LOAD_NO_MEMORY;
  }

  /*
   * makes sel_ldr think that the module's address space is at 0x0, this where
   * it should be
   */
  *mem = 0x0;

  return LOAD_OK;
}

NaClErrorCode NaClMprotectGuards(struct NaClApp *nap, uintptr_t start_addr) {
  int err;
  void *guard_base = (void *)(1 << nap->addr_bits);
  /*
   * In ARM implementation kernel does not allow us to mmap address space at
   * address 0x0, so we mmap it at the start of a trampoline region.
   * Therefore, there is not need to mprotect at the start_addr.
   */
  UNREFERENCED_PARAMETER(start_addr);

  /*
   * Instead, we create a two-page guard region at the base of trusted
   * memory.
   */
  if ((err = NaCl_mprotect(guard_base, 2 * NACL_PAGESIZE, PROT_NONE)) != 0) {
    NaClLog(LOG_ERROR, ("NaClMemoryProtection: failed to protect lower guard "
                        "on trusted memory space (error %d)\n"),
            err);
    return LOAD_MPROTECT_FAIL;
  }

  if (!NaClVmmapAdd(&nap->mem_map,
                    ((uintptr_t) guard_base - nap->mem_start) >> NACL_PAGESHIFT,
                    2 * NACL_PAGESIZE,
                    PROT_NONE,
                    (struct NaClMemObj *) NULL)) {
    NaClLog(LOG_ERROR, ("NaClMemoryPRotection: NaClVmmapAdd failed"
                        " (lower guard on trusted memory space)\n"));
    return LOAD_MPROTECT_FAIL;
  }
  return LOAD_OK;
}

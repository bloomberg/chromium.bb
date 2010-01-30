/*
 * Copyright 2009 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <errno.h>

#include "native_client/src/include/nacl_platform.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/service_runtime/sel_memory.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"

#define ALIGN_BITS  32
#define FOURGIG     (((size_t) 1) << 32)
#define FOURKAY     (((size_t) 1) << 12)
#define MSGWIDTH    "25"

int NaClAllocateSpace(void **mem, size_t size) {
  size_t  mem_sz = 21 * FOURGIG;  /* 40G guard on each side */
  size_t  log_align = ALIGN_BITS;
  void    *mem_ptr;

  NaClLog(LOG_INFO, "NaClAllocateSpace(*, 0x%016"PRIxS" bytes).\n", size);

  CHECK(size == FOURGIG);

  errno = 0;
  mem_ptr = NaClAllocatePow2AlignedMemory(mem_sz, log_align);
  if (NULL == mem_ptr) {
    if (0 != errno) {
      perror("NaClAllocatePow2AlignedMemory");
    }
    NaClLog(LOG_WARNING, "Memory allocation failed\n");

    return LOAD_NO_MEMORY;
  }
  /*
   * The module lives in the middle FOURGIG of the allocated region --
   * we skip over an initial 40G guard.
   */
  *mem = (void *) (((char *) mem_ptr) + 10 * FOURGIG);
  NaClLog(LOG_INFO,
          "NaClAllocateSpace: addr space at 0x%016"PRIxPTR"\n",
          (uintptr_t) *mem);

  return LOAD_OK;
}

NaClErrorCode NaClMprotectGuards(struct NaClApp *nap) {
  uintptr_t start_addr;
  int       err;

  start_addr = nap->mem_start;

  NaClLog(3,
          ("NULL detection region start 0x%08"PRIxPTR
           ", size 0x%08x, end 0x%08"PRIxPTR"\n"),
          start_addr, NACL_SYSCALL_START_ADDR,
          start_addr + NACL_SYSCALL_START_ADDR);
  if ((err = NaCl_mprotect((void *) start_addr,
                           NACL_SYSCALL_START_ADDR,
                           PROT_NONE)) != 0) {
    NaClLog(LOG_ERROR,
            ("NaClMprotectGuards:"
             " NaCl_mprotect(0x%016"PRIxPTR", 0x%016x, 0x%x) failed,"
             " error %d (NULL pointer guard page)\n"),
            start_addr, NACL_SYSCALL_START_ADDR, PROT_NONE,
            err);
    return LOAD_MPROTECT_FAIL;
  }
  if (!NaClVmmapAdd(&nap->mem_map,
                    (start_addr - nap->mem_start) >> NACL_PAGESHIFT,
                    NACL_SYSCALL_START_ADDR >> NACL_PAGESHIFT,
                    PROT_NONE,
                    (struct NaClMemObj *) NULL)) {
    NaClLog(LOG_ERROR, ("NaClMprotectGuards: NaClVmmapAdd failed"
                        " (NULL pointer guard page)\n"));
    return LOAD_MPROTECT_FAIL;
  }

  /*
   * Now add additional guard pages for write protection.  We have 40G
   * of address space on either side of the main 4G address space that
   * we have to make inaccessible....
   */
  if ((err = NaCl_mprotect((void *) (start_addr - 10 * FOURGIG),
                           10 * FOURGIG,
                           PROT_NONE)) != 0) {
    NaClLog(LOG_ERROR,
            ("NaClMprotectGuards:"
             " NaCl_mprotect(0x%016"PRIxPTR", 0x%016"PRIxS", 0x%x) failed."
             " error %d (pre-address-space guard)\n"),
            (start_addr - 10 * FOURGIG),
            10 * FOURGIG,
            PROT_NONE,
            err);
    return LOAD_MPROTECT_FAIL;
  }
  if ((err = NaCl_mprotect((void *) (start_addr + FOURGIG),
                           10 * FOURGIG,
                           PROT_NONE)) != 0) {
    NaClLog(LOG_ERROR,
            ("NaClMprotectGuards:"
             " NaCl_mprotect(0x%016"PRIxPTR", 0x%016"PRIxS", 0x%x) failed."
             " error %d (post-address-space guard)\n"),
            (start_addr + FOURGIG),
            10 * FOURGIG,
            PROT_NONE,
            err);
    return LOAD_MPROTECT_FAIL;
  }

  return LOAD_OK;
}

void NaClTeardownMprotectGuards(struct NaClApp *nap) {
  uintptr_t start_addr;

  start_addr = nap->mem_start;

  /*
   * No need to undo mprotect, since we're just deallocating back to
   * the system.
   */
  NaCl_page_free((void *) (start_addr - 10 * FOURGIG),
                 10 * FOURGIG);
  NaCl_page_free((void *) (start_addr + FOURGIG),
                 10 * FOURGIG);
}

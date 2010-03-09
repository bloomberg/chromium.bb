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
#define GUARDSIZE   (10 * FOURGIG)
#define MSGWIDTH    "25"

NaClErrorCode NaClAllocateSpace(void **mem, size_t addrsp_size) {
  size_t        mem_sz = 2 * GUARDSIZE + FOURGIG;  /* 40G guard on each side */
  size_t        log_align = ALIGN_BITS;
  void          *mem_ptr;
  NaClErrorCode err = LOAD_INTERNAL;

  NaClLog(LOG_INFO, "NaClAllocateSpace(*, 0x%016"NACL_PRIxS" bytes).\n",
          addrsp_size);

  CHECK(addrsp_size == FOURGIG);

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
  *mem = (void *) (((char *) mem_ptr) + GUARDSIZE);
  NaClLog(LOG_INFO,
          "NaClAllocateSpace: addr space at 0x%016"NACL_PRIxPTR"\n",
          (uintptr_t) *mem);

#if NACL_WINDOWS
  /*
   * On Windows, the newly allocated memory starts out uncommitted, which
   * is equivalent to PROT_NONE page protection. To get the expected
   * behavior, we need to start out with our address space (but not the
   * guard pages) set to PROT_READ|WRITE. On other platforms this should
   * be a no-op.
   */
  err = NaCl_mprotect((*mem), addrsp_size, PROT_READ|PROT_WRITE);
#else
  err = LOAD_OK;
#endif

  return err;
}

NaClErrorCode NaClMprotectGuards(struct NaClApp *nap) {
  uintptr_t start_addr;
  int       err;
  void      *guard[2];

  start_addr = nap->mem_start;
  guard[0] = (void *)(start_addr - GUARDSIZE);
  guard[1] = (void *)(start_addr + FOURGIG);

  NaClLog(3,
          ("NULL detection region start 0x%08"NACL_PRIxPTR", "
           "size 0x%08x, end 0x%08"NACL_PRIxPTR"\n"),
          start_addr, NACL_SYSCALL_START_ADDR,
          start_addr + NACL_SYSCALL_START_ADDR);
  if ((err = NaCl_mprotect((void *) start_addr,
                           NACL_SYSCALL_START_ADDR,
                           PROT_NONE)) != 0) {
    NaClLog(LOG_ERROR,
            ("NaClMprotectGuards:"
             " NaCl_mprotect(0x%016"NACL_PRIxPTR", 0x%016x, 0x%x) failed,"
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
#if !NACL_WINDOWS
  /*
   * On Windows we do not alter the page protection of the guard regions,
   * as this would require them to be committed. Instead we keep them
   * reserved but uncommitted, which means that access attempts will fault.
   * On other systems, we mprotect the guards.
   */
  if ((err = NaCl_mprotect((void *) (start_addr - GUARDSIZE),
                           GUARDSIZE,
                           PROT_NONE)) != 0) {
    NaClLog(LOG_ERROR,
            ("NaClMprotectGuards: "
             "NaCl_mprotect(0x%016"NACL_PRIxPTR", "
             "0x%016"NACL_PRIxS", 0x%x) failed."
             " error %d (pre-address-space guard)\n"),
            (start_addr - GUARDSIZE),
            GUARDSIZE,
            PROT_NONE,
            err);
    return LOAD_MPROTECT_FAIL;
  }
  if ((err = NaCl_mprotect((void *) (start_addr + FOURGIG),
                           GUARDSIZE,
                           PROT_NONE)) != 0) {
    NaClLog(LOG_ERROR,
            ("NaClMprotectGuards:"
             " NaCl_mprotect(0x%016"NACL_PRIxPTR", "
             "0x%016"NACL_PRIxS", 0x%x) failed."
             " error %d (post-address-space guard)\n"),
            (start_addr + FOURGIG),
            GUARDSIZE,
            PROT_NONE,
            err);
    return LOAD_MPROTECT_FAIL;
  }
#endif
  return LOAD_OK;
}

void NaClTeardownMprotectGuards(struct NaClApp *nap) {
  uintptr_t start_addr;

  if (!nap->guard_pages_initialized) {
    NaClLog(4, "No guard pages to tear down.\n");
    return;
  }
  start_addr = nap->mem_start;

  /*
   * No need to undo mprotect, since we're just deallocating back to
   * the system.
   */
  NaCl_page_free((void *) (start_addr - GUARDSIZE),
                 GUARDSIZE);
  NaCl_page_free((void *) (start_addr + FOURGIG),
                 GUARDSIZE);
}

/*
 * Copyright 2008, Google Inc.
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

/* NaCl Simple/secure ELF loader (NaCl SEL).
 */

#include "native_client/src/include/nacl_platform.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/service_runtime/sel_addrspace.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/service_runtime/sel_memory.h"
#include "native_client/src/trusted/service_runtime/sel_util.h"


NaClErrorCode NaClAllocAddrSpace(struct NaClApp *nap) {
  void        *mem;
  int         rv;
  uintptr_t   hole_start;
  size_t      hole_size;
  uintptr_t   stack_start;

  NaClLog(2, "NaClAllocAddrSpace: calling NaCl_page_alloc(*,0x%x)\n",
          (1U << nap->addr_bits));

  rv = NaClAllocateSpace(&mem, 1U << nap->addr_bits);
  if (rv != LOAD_OK) return rv;

  nap->mem_start = (uintptr_t) mem;
  nap->xlate_base = nap->mem_start;
  NaClLog(2, "allocated memory at 0x%08"PRIxPTR"\n", nap->mem_start);

  hole_start = NaClRoundAllocPage(nap->data_end);

  /* TODO(bsy): check for underflow?  only trusted code can set stack_size */
  stack_start = (1U << nap->addr_bits) - nap->stack_size;
  stack_start = NaClTruncAllocPage(stack_start);

  if (stack_start < hole_start) {
    return LOAD_DATA_OVERLAPS_STACK_SECTION;
  }

  hole_size = stack_start - hole_start;
  hole_size = NaClTruncAllocPage(hole_size);

  /*
   * mprotect and madvise unused data space to "free" it up, but
   * retain mapping so no other memory can be mapped into those
   * addresses.
   */
  if (hole_size == 0) {
    NaClLog(2, ("NaClAllocAddrSpace: hole between end of data and"
                " the beginning of stack is zero size.\n"));
  } else {
    NaClLog(2,
            "madvising 0x%08"PRIxPTR", 0x%08"PRIxS", PROT_NONE\n",
            nap->mem_start + hole_start, hole_size);
    if (NaCl_madvise((void *) (nap->mem_start + hole_start),
                     hole_size,
                     MADV_DONTNEED) != 0) {
      return LOAD_MADVISE_FAIL;
    }

    NaClLog(2,
            "mprotecting 0x%08"PRIxPTR", 0x%08"PRIxS", PROT_NONE\n",
            nap->mem_start + hole_start, hole_size);
    if (NaCl_mprotect((void *) (nap->mem_start + hole_start),
                      hole_size,
                      PROT_NONE) != 0) {
      return LOAD_MPROTECT_FAIL;
    }
  }

  return LOAD_OK;
}


/*
 * Apply memory protection to memory regions.
 */
NaClErrorCode NaClMemoryProtection(struct NaClApp *nap) {
  uintptr_t start_addr;
  uint32_t  region_size;
  int       err;

  start_addr = nap->mem_start;

  /*
   * The first NACL_SYSCALL_START_ADDR bytes are mapped as PROT_NONE.
   * This enables NULL pointer checking, and provides additional protection
   * against addr16/data16 prefixed operations being used for attacks.
   * Since NACL_SYSCALL_START_ADDR is a multiple of the page size, we don't
   * need to round it to be so.
   */

  err = NaClMprotectGuards(nap, start_addr);
  if (err != LOAD_OK) return err;

  start_addr = nap->mem_start + NACL_SYSCALL_START_ADDR;
  /*
   * The next pages up to NACL_TRAMPOLINE_END are the trampolines.
   * Immediately following that is the loaded text section.
   * These are collectively marked as PROT_READ | PROT_EXEC.
   */
  region_size = NaClRoundPage(NACL_TRAMPOLINE_END - NACL_SYSCALL_START_ADDR
                              + nap->text_region_bytes);
  NaClLog(3,
          ("Trampoline/text region start 0x%08"PRIxPTR","
           " size 0x%08"PRIx32", end 0x%08"PRIxPTR"\n"),
          start_addr, region_size,
          start_addr + region_size);
  if ((err = NaCl_mprotect((void *) start_addr,
                           region_size,
                           PROT_READ | PROT_EXEC)) != 0) {
    NaClLog(LOG_ERROR,
            ("NaClMemoryProtection:"
             " NaCl_mprotect(0x%08"PRIxPTR", 0x%08"PRIx32", 0x%x) failed,"
             " error %d (trampoline)\n"),
            start_addr, region_size, PROT_READ | PROT_EXEC,
            err);
    return LOAD_MPROTECT_FAIL;
  }
  if (!NaClVmmapAdd(&nap->mem_map,
                    (start_addr - nap->mem_start) >> NACL_PAGESHIFT,
                    region_size >> NACL_PAGESHIFT,
                    PROT_READ | PROT_EXEC,
                    (struct NaClMemObj *) NULL)) {
    NaClLog(LOG_ERROR, ("NaClMemoryProtection: NaClVmmapAdd failed"
                        " (trampoline)\n"));
    return LOAD_MPROTECT_FAIL;
  }

  start_addr = NaClRoundPage(start_addr + region_size);
  /*
   * data_end is max virtual addr seen, so start_addr <= data_end
   * must hold.
   */

  region_size = NaClRoundPage(NaClRoundAllocPage(nap->data_end)
                              + nap->mem_start - start_addr);
  NaClLog(3,
          ("RW data region start 0x%08"PRIxPTR", size 0x%08"PRIx32","
           " end 0x%08"PRIxPTR"\n"),
          start_addr, region_size,
          start_addr + region_size);
  if ((err = NaCl_mprotect((void *) start_addr,
                           region_size,
                           PROT_READ | PROT_WRITE)) != 0) {
    NaClLog(LOG_ERROR,
            ("NaClMemoryProtection:"
             " NaCl_mprotect(0x%08"PRIxPTR", 0x%08"PRIx32", 0x%x) failed,"
             " error %d (data)\n"),
            start_addr, region_size, PROT_READ | PROT_WRITE,
            err);
    return LOAD_MPROTECT_FAIL;
  }
  if (!NaClVmmapAdd(&nap->mem_map,
                    (start_addr - nap->mem_start) >> NACL_PAGESHIFT,
                    region_size >> NACL_PAGESHIFT,
                    PROT_READ | PROT_WRITE,
                    (struct NaClMemObj *) NULL)) {
    NaClLog(LOG_ERROR, ("NaClMemoryProtection: NaClVmmapAdd failed"
                        " (data)\n"));
    return LOAD_MPROTECT_FAIL;
  }

  /* stack is read/write but not execute */
  region_size = nap->stack_size;
  start_addr = (nap->mem_start
                + NaClTruncAllocPage((1U << nap->addr_bits)
                                     - nap->stack_size));
  NaClLog(3,
          ("RW stack region start 0x%08"PRIxPTR", size 0x%08"PRIx32","
           " end 0x%08"PRIxPTR"\n"),
          start_addr, region_size,
          start_addr + region_size);
  if ((err = NaCl_mprotect((void *) start_addr,
                           NaClRoundAllocPage(nap->stack_size),
                           PROT_READ | PROT_WRITE)) != 0) {
    NaClLog(LOG_ERROR,
            ("NaClMemoryProtection:"
             " NaCl_mprotect(0x%08"PRIxPTR", 0x%08"PRIx32", 0x%x) failed,"
             " error %d (stack)\n"),
            start_addr, region_size, PROT_READ | PROT_WRITE,
            err);
    return LOAD_MPROTECT_FAIL;
  }

  if (!NaClVmmapAdd(&nap->mem_map,
                    (start_addr - nap->mem_start) >> NACL_PAGESHIFT,
                    nap->stack_size >> NACL_PAGESHIFT,
                    PROT_READ | PROT_WRITE,
                    (struct NaClMemObj *) NULL)) {
    NaClLog(LOG_ERROR, ("NaClMemoryProtection: NaClVmmapAdd failed"
                        " (stack)\n"));
    return LOAD_MPROTECT_FAIL;
  }
  return LOAD_OK;
}

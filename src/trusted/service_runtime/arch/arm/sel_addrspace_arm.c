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

#include "native_client/src/include/nacl_platform.h"
#include "native_client/src/trusted/service_runtime/nacl_check.h"
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


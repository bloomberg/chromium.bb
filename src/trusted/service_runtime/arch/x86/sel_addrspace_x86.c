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
#include "native_client/src/trusted/service_runtime/sel_memory.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"


int NaClAllocateSpace(void **mem, size_t size) {
  CHECK(mem);
  if (NaCl_page_alloc(mem, size) != 0) {
    NaClLog(2, "NaClAlloccaterSpace: NaCl_page_alloc failed\n");
    return LOAD_NO_MEMORY;
  }
  return LOAD_OK;
}


NaClErrorCode NaClMprotectGuards(struct NaClApp *nap, uintptr_t start_addr) {
  int err;

  NaClLog(3,
          ("NULL detection region start 0x%08"PRIxPTR
           ", size 0x%08x, end 0x%08"PRIxPTR"\n"),
          start_addr, NACL_SYSCALL_START_ADDR,
          start_addr + NACL_SYSCALL_START_ADDR);
  if ((err = NaCl_mprotect((void *) start_addr,
                           NACL_SYSCALL_START_ADDR,
                           PROT_NONE)) != 0) {
    NaClLog(LOG_ERROR, ("NaClMemoryProtection:"
                        " NaCl_mprotect(0x%08"PRIxPTR", 0x%08x, 0x%x) failed,"
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
    NaClLog(LOG_ERROR, ("NaClMemoryProtection: NaClVmmapAdd failed"
                        " (NULL pointer guard page)\n"));
    return LOAD_MPROTECT_FAIL;
  }

  return LOAD_OK;
}


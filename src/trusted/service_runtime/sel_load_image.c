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

/*
 * NaCl Simple/secure ELF loader (NaCl SEL).
 */

#include <string.h>

#include "native_client/src/include/nacl_elf.h"
#include "native_client/src/trusted/platform/nacl_log.h"
#include "native_client/src/trusted/service_runtime/nacl_config.h"
#include "native_client/src/trusted/service_runtime/nacl_check.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/service_runtime/sel_util.h"
#include "native_client/src/trusted/service_runtime/tramp.h"

NaClErrorCode NaClLoadImage(struct Gio     *gp,
                            struct NaClApp *nap) {
  int                                 segnum;
  Elf32_Phdr                          *php;
  uintptr_t                           paddr;
  uintptr_t                           end_vaddr;
  size_t                              page_pad;

  for (segnum = 0; segnum < nap->elf_hdr.e_phnum; ++segnum) {
    php = &nap->phdrs[segnum];

    /* did we decide that we will load this segment earlier? */
    if (0 == (php->p_flags & PF_OS_WILL_LOAD)) {
      continue;
    }

    end_vaddr = php->p_vaddr + php->p_filesz;
    /* integer overflow? */
    if (end_vaddr < php->p_vaddr) {
      NaClLog(LOG_FATAL, "parameter error should have been detected already\n");
    }
    /*
     * is the end virtual address within the NaCl application's
     * address space?  if it is, it implies that the start virtual
     * address is also.
     */
    if (end_vaddr >= (1U << nap->addr_bits)) {
      NaClLog(LOG_FATAL, "parameter error should have been detected already\n");
    }

    paddr = nap->mem_start + php->p_vaddr;

    if ((*gp->vtbl->Seek)(gp, php->p_offset, SEEK_SET) == -1) {
      return LOAD_SEGMENT_BAD_PARAM;
    }
    if ((Elf32_Word) (*gp->vtbl->Read)(gp, (void *) paddr, php->p_filesz)
        != php->p_filesz) {
      return LOAD_SEGMENT_BAD_PARAM;
    }
    /* region from p_filesz to p_memsz should already be zero filled */
  }

  /*
   * fill from text_region_bytes to end of that page with halt
   * instruction, which is one byte in size.
   */
  page_pad = NaClRoundPage(nap->text_region_bytes) - nap->text_region_bytes;
  CHECK(page_pad < NACL_PAGESIZE);

  memset((void *) (nap->mem_start
                   + NACL_TRAMPOLINE_END
                   + nap->text_region_bytes),
         NACL_HALT_OPCODE,
         page_pad);
  nap->text_region_bytes += page_pad;

  return LOAD_OK;
}

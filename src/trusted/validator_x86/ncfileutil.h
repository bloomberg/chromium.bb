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
 * ncfileutil.h - open an executable file. FOR TESTING ONLY.
 */
#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCFILEUTIL_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCFILEUTIL_H_

#include "native_client/src/include/portability.h"
#include "native_client/src/include/nacl_elf.h"
#include "native_client/src/trusted/validator_x86/types_memory_model.h"

EXTERN_C_BEGIN

/* memory access permissions have a granularity of 4KB pages. */
static const int kNCFileUtilPageShift =      12;
static const Elf_Addr kNCFileUtilPageSize = (1 << 12);
/* this needs to be a #define because it's used as an array size */
#if NACL_TARGET_SUBARCH == 64
#define kMaxPhnum 64
#else
#define kMaxPhnum 32
#endif

typedef struct {
  const char* fname; /* name of loaded file */
  PcAddress vbase;   /* base address in virtual memory */
  MemorySize size;   /* size of program memory         */
  uint8_t* data;     /* the actual loaded bytes        */
  Elf_Half phnum;  /* number of Elf program headers */
  Elf_Phdr* pheaders;  /* copy of the Elf program headers */
  Elf_Half shnum;  /* number of Elf section headers */
  Elf_Shdr* sheaders;  /* copy of the Elf section headers */
  int ncalign;
} ncfile;

/* Loads the given filename into memory, and if the nc_rules is non-zero,
 * require native client rules to be applied.
 */
ncfile *nc_loadfile_depending(const char* filename, int nc_rules);

/* Loads the given filename into memory, applying native client rules. */
static INLINE ncfile *nc_loadfile(const char *filename) {
  return nc_loadfile_depending(filename, 1);
}

void nc_freefile(ncfile* ncf);

void GetVBaseAndLimit(ncfile* ncf, PcAddress* vbase, PcAddress* vlimit);

EXTERN_C_END

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCFILEUTIL_H_ */

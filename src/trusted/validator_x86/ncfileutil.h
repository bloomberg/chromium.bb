/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
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
  const char* fname;     /* name of loaded file */
  NaClPcAddress vbase;   /* base address in virtual memory */
  NaClMemorySize size;   /* size of program memory         */
  uint8_t* data;         /* the actual loaded bytes        */
  Elf_Half phnum;        /* number of Elf program headers */
  Elf_Phdr* pheaders;    /* copy of the Elf program headers */
  Elf_Half shnum;        /* number of Elf section headers */
  Elf_Shdr* sheaders;    /* copy of the Elf section headers */
  int ncalign;
} ncfile;

/* Loads the given filename into memory, and if the nc_rules is non-zero,
 * require native client rules to be applied.
 */
ncfile *nc_loadfile_depending(const char* filename, int nc_rules);

/* Loads the given filename into memory, applying native client rules. */
ncfile *nc_loadfile(const char *filename);

void nc_freefile(ncfile* ncf);

void GetVBaseAndLimit(ncfile* ncf, NaClPcAddress* vbase, NaClPcAddress* vlimit);

/* Function signature for error printing. */
typedef void (*nc_loadfile_error_fn)(const char* format,
                                     ...) ATTRIBUTE_FORMAT_PRINTF(1, 2);

/* Register error print function to use. Returns the old print fcn.
 * Warning: this function is not threadsafe. Caller is responsible for
 * locking updates if multiple threads call this function.
 */
nc_loadfile_error_fn NcLoadFileRegisterErrorFn(nc_loadfile_error_fn fn);

/* Define the default print error function to use for this module. */
void NcLoadFilePrintError(const char* format,
                          ...) ATTRIBUTE_FORMAT_PRINTF(1, 2);

EXTERN_C_END

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCFILEUTIL_H_ */

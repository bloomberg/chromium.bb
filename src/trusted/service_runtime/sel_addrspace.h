/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * NaCl Simple/secure ELF loader (NaCl SEL).
 */

#ifndef __SEL_ADDRSPACE_H__
#define __SEL_ADDRSPACE_H__ 1

#include "native_client/src/include/portability.h"
#include "native_client/src/trusted/service_runtime/nacl_error_code.h"

struct NaClApp; /* fwd */

NaClErrorCode NaClAllocAddrSpace(struct NaClApp *nap) NACL_WUR;

/*
 * Apply memory protection to memory regions.
 */
NaClErrorCode NaClMemoryProtection(struct NaClApp *nap) NACL_WUR;

int NaClAllocateSpace(void **mem, size_t size) NACL_WUR;

NaClErrorCode NaClMprotectGuards(struct NaClApp *nap);

void NaClTeardownMprotectGuards(struct NaClApp *nap);
#endif

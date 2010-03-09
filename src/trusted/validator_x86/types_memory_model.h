/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/* types_memory_model.h - Model memory addresses and memory size.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_x86_TYPES_MEMORY_MODEL_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_x86_TYPES_MEMORY_MODEL_H_

#include "native_client/src/include/portability.h"

/* Define the width of an address based on the wordsize.
 * PcAddress - An address into memory.
 * PcNumber - Number to allow the computation of relative address offsets
 *            (both positive and negative).
 * MemorySize - The number of bytes in memory.
 */
#if NACL_TARGET_SUBARCH == 64
typedef uint64_t PcAddress;
#define NACL_PRIxPcAddress    NACL_PRIx64
#define NACL_PRIxPcAddressAll "016"NACL_PRIx64

typedef int64_t PcNumber;
#define NACL_PRIdPcNumber NACL_PRId64

typedef uint64_t MemorySize;
#define NACL_PRIxMemorySize NACL_PRIx64
#else
typedef uint32_t PcAddress;
#define NACL_PRIxPcAddress     NACL_PRIx32
#define NACL_PRIxPcAddressAll "08"NACL_PRIx32

typedef int32_t PcNumber;
#define NACL_PRIdPcNumber NACL_PRId32

typedef uint32_t MemorySize;
#define NACL_PRIxMemorySize NACL_PRIx32
#endif

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_x86_TYPES_MEMORY_MODEL_H_ */

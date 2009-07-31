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

/* addresses.h - Model memory addresses and memory size.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_x86_ADDRESSES_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_x86_ADDRESSES_H_

/* Define the width of an address based on the wordsize.
 * PcAddress - An address into memory.
 * PcNumber - Number to allow the computation of relative address offsets
 *            (both positive and negative).
 * MemorySize - The number of bytes in memory.
 */
#if NACL_TARGET_SUBARCH == 64
typedef uint64_t PcAddress;
#define PRIxPcAddress    PRIx64
#define PRIxPcAddressAll "016"PRIx64

typedef int64_t PcNumber;

typedef uint64_t MemorySize;
#define PRIxMemorySize PRIx64
#else
typedef uint32_t PcAddress;
#define PRIxPcAddress     PRIx32
#define PRIxPcAddressAll "08"PRIx32

typedef int32_t PcNumber;

typedef uint32_t MemorySize;
#define PRIxMemorySize PRIx32
#endif

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_x86_ADDRESSES_H_ */

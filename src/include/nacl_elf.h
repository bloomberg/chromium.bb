/*
 * nacl_elf.h
 * NativeClient
 */
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
 * TODO(jrg): what's the right location for this file?
 * It's needed to compile both  ncv and service_runtime.
 */

#ifndef NATIVE_CLIENT_SRC_INCLUDE_NACL_ELF_H_
#define NATIVE_CLIENT_SRC_INCLUDE_NACL_ELF_H_ 1

#if !defined(NACL_LINUX) && !defined(NACL_OSX) && !defined(NACL_WINDOWS)
#error "Bad NaCL build."
#endif

#include "native_client/src/include/elf.h"

/*
 * The ELF OS ABI marker for a NativeClient module.
 * TODO(sehr): Get real values approved by the appropriate authorities.
 */
#define ELFOSABI_NACL 123

/* e_flags settings for NativeClient. */
#define EF_NACL_ALIGN_MASK  0x300000  /* bits indicating alignment */
#define EF_NACL_ALIGN_16    0x100000  /* aligned zero mod 16 */
#define EF_NACL_ALIGN_32    0x200000  /* aligned zero mod 32 */
#define EF_NACL_ALIGN_LIB   0x000000  /* aligned to pass either way */

/*
 * ABI version - increment when ABI changes are incompatible.
 * note: also bump the same EF_NACL_ABIVERSION define in:
 *   googleclient/native_client/tools/patches/binutils-2.18.patch
 */
#define EF_NACL_ABIVERSION 7
#endif  /* NATIVE_CLIENT_SRC_INCLUDE_NACL_ELF_H_ */

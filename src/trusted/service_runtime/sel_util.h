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
 * NaCl Simple/secure ELF loader (NaCl SEL) misc utilities.
 */
#ifndef NATIVE_CLIENT_SERVICE_RUNTIME_SEL_UTIL_H_
#define NATIVE_CLIENT_SERVICE_RUNTIME_SEL_UTIL_H_ 1

#include "native_client/src/include/portability.h"
#include "native_client/src/include/nacl_base.h"

#include <sys/types.h>

#include "native_client/src/trusted/service_runtime/nacl_config.h"

EXTERN_C_BEGIN

size_t  NaClAppPow2Ceil(size_t  max_addr);

/*
 * NaClRoundPage is a bit of a misnomer -- it always rounds up to a
 * page size, not the nearest.
 */
static INLINE size_t  NaClRoundPage(size_t    nbytes) {
  return (nbytes + NACL_PAGESIZE - 1) & ~(NACL_PAGESIZE - 1);
}

static INLINE size_t  NaClRoundAllocPage(size_t    nbytes) {
  return (nbytes + NACL_MAP_PAGESIZE - 1) & ~(NACL_MAP_PAGESIZE - 1);
}

static INLINE size_t NaClTruncPage(size_t  nbytes) {
  return nbytes & ~(NACL_PAGESIZE - 1);
}

static INLINE size_t NaClTruncAllocPage(size_t  nbytes) {
  return nbytes & ~(NACL_MAP_PAGESIZE - 1);
}

static INLINE size_t  NaClBytesToPages(size_t nbytes) {
  return (nbytes + NACL_PAGESIZE - 1) >> NACL_PAGESHIFT;
}

static INLINE int /* bool */ NaClIsPageMultiple(uintptr_t addr_or_size) {
  return 0 == ((NACL_PAGESIZE - 1) & addr_or_size);
}

static INLINE int /* bool */ NaClIsAllocPageMultiple(uintptr_t addr_or_size) {
  return 0 == ((NACL_MAP_PAGESIZE - 1) & addr_or_size);
}

/*
 * True host-OS allocation unit.
 */
static INLINE size_t NaClRoundHostAllocPage(size_t  nbytes) {
#if NACL_WINDOWS
  return NaClRoundAllocPage(nbytes);
#else   /* NACL_WINDOWS */
  return NaClRoundPage(nbytes);
#endif  /* !NACL_WINDOWS */
}

static INLINE size_t NaClRoundPageNumUpToMapMultiple(size_t npages) {
  return (npages + NACL_PAGES_PER_MAP - 1) & ~(NACL_PAGES_PER_MAP - 1);
}

static INLINE size_t NaClTruncPageNumDownToMapMultiple(size_t npages) {
  return npages & ~(NACL_PAGES_PER_MAP - 1);
}

EXTERN_C_END

#endif  /* NATIVE_CLIENT_SERVICE_RUNTIME_SEL_UTIL_H_ */

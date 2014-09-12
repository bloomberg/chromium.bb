/*
 * Copyright (c) 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_INCLUDE_MINSFI_PTR_H_
#define NATIVE_CLIENT_SRC_INCLUDE_MINSFI_PTR_H_

#include <assert.h>
#include <string.h>

#include "native_client/src/include/minsfi_priv.h"

/*
 * This defines what ToMinsfiPtr should do if the given native pointer
 * is not within the bounds of the allocated memory region. We abort in
 * production code and return a magic constant when invoked inside tests.
 */
#ifndef MINSFI_PTR_CONVERSION_TEST
#define ToMinsfiPtr_AbortAction    abort();
#else  // MINSFI_PTR_CONVERSION_TEST
#define ToMinsfiPtr_AbortAction    return 0xCAFEBABE;
#endif  // MINSFI_PTR_CONVERSION_TEST

/*
 * Convert an untrusted pointer into a native pointer. Because this is just
 * some integer provided by the untrusted code, we must sandbox it the same
 * way the SandboxMemoryAccesses compiler pass does.
 */
static inline char *FromMinsfiPtr(sfiptr_t sfiptr, const MinsfiSandbox *sb) {
  uint32_t masked_sfiptr = sfiptr & sb->ptr_mask;
  return sb->mem_base + masked_sfiptr;
}

/*
 * Convert a native pointer into an untrusted pointer. This means simply
 * subtracting the memory base from the address.
 */
static inline sfiptr_t ToMinsfiPtr(const char *ptr, const MinsfiSandbox *sb) {
  uintptr_t ptr_int = (uintptr_t) ptr;
  uintptr_t base_int = (uintptr_t) sb->mem_base;
  sfiptr_t sb_ptr = ptr_int - base_int;

  /* Check that the pointer is in the bounds of the allocated memory region. */
  if ((base_int > ptr_int) || ((sb_ptr & (~sb->ptr_mask)) != 0)) {
    ToMinsfiPtr_AbortAction
  }

  return sb_ptr;
}

/*
 * Copy data into the untrusted memory region. We must sandbox both the
 * untrusted pointer and the data length in order to guarantee that memcpy
 * will not write beyond the memory mapped for the sandbox. This is analogous
 * to the sandboxing of memory intrinsics in the SandboxMemoryAccesses compiler
 * pass.
 *
 * Note that this is not a thread-safe operation.
 */
static inline void MinsfiCopyToSandbox(sfiptr_t sb_to, const void *from,
                                       uint32_t len, const MinsfiSandbox *sb) {
  uint32_t sandboxed_len = len & sb->ptr_mask;
  memcpy(FromMinsfiPtr(sb_to, sb), from, sandboxed_len);
}

#endif  // NATIVE_CLIENT_SRC_INCLUDE_MINSFI_PTR_H_

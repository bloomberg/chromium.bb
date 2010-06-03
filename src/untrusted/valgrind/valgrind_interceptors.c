/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/* This file contains valgrind interceptors for NaCl's untrusted library
   functions such as malloc, free, etc.
   When running under valgrind, the function named foo() will be replaced
   with a function from this file named I_WRAP_SONAME_FNNAME_ZZ(NONE, foo).
   The latter function may in turn call the original foo().

   For details about valgrind interceptors (function wrapping) refer to
     http://valgrind.org/docs/manual/manual-core-adv.html

   In order to use this library, link with the following gcc flags:
     -lvalgrind -Wl,-u,have_nacl_valgrind_interceptors

   TODO(kcc): extend the interceptors to fully cover memcheck needs:
       calloc/realloc/operator new/operator delete/etc
   TODO(kcc): extend the interceptors to fully cover ThreadSanitizer needs:
       pthread_mutex_.../pthread_rwlock_.../sem_.../etc
 */

#include <stdio.h>
#include <assert.h>

/* Currently, valgrind-on-nacl is supported only for x86-64 linux. */
#if defined(__linux__) && defined(__x86_64__)

#include "native_client/src/third_party/valgrind/nacl_valgrind.h"
#include "native_client/src/third_party/valgrind/nacl_memcheck.h"

#define INLINE __attribute__((always_inline))

/* This variable needs to be referenced by a program (either in sources,
  or using the linker flag -u) to which this library is linked.
  When using gcc/g++ as a linker, use -Wl,-u,have_nacl_valgrind_interceptors.
*/
int have_nacl_valgrind_interceptors;

/* Create red zones of this size around malloc-ed memory.
 Must be >= 3*sizeof(size_t) */
static const int kRedZoneSize = 32;
/* Used for sanity checking. */
static const size_t kMallocMagic = 0x1234abcd;

/* We need to delay the reuse of free-ed memory so that memcheck can report
 the uses of free-ed memory with detailed stacks.
 When a pointer is passed to free(), we put it into this FIFO queue.
 Instead of free-ing this pointer instantly, we free a pointer
 in the back of the queue.
 */
#define DELAY_REUSE_QUEUE_SIZE 1024
static size_t delay_reuse_queue[DELAY_REUSE_QUEUE_SIZE];
static size_t drq_begin;

/* Generic malloc() handler. */
INLINE static size_t handle_malloc(OrigFn fn, size_t size) {
  size_t ptr;
  uint64_t base;
  /* Allocate memory with red zones fro both sides. */
  CALL_FN_W_W(ptr, fn, size + 2 * kRedZoneSize);
  /* Mark all memory as defined, put our own data at the beginning. */
  base = VALGRIND_SANDBOX_PTR(ptr);
  VALGRIND_MAKE_MEM_DEFINED(base, kRedZoneSize);
  ((size_t*)ptr)[0] = kMallocMagic;
  ((size_t*)ptr)[1] = size;
  ((size_t*)ptr)[2] = kMallocMagic;
  /* Tell memcheck about malloc-ed memory and red-zones. */
  VALGRIND_MALLOCLIKE_BLOCK(base + kRedZoneSize, size, kRedZoneSize, 0);
  VALGRIND_MAKE_MEM_NOACCESS(base, kRedZoneSize);
  VALGRIND_MAKE_MEM_NOACCESS(base + kRedZoneSize + size, kRedZoneSize);
  /* Done */
  return ptr + kRedZoneSize;
}

/* Generic free() handler. */
INLINE static void handle_free(OrigFn fn, size_t ptr) {
  uint64_t base;
  size_t size, old_ptr;
  /* Get the size of allocated region, check sanity. */
  ptr -= kRedZoneSize;
  base = VALGRIND_SANDBOX_PTR(ptr);
  VALGRIND_MAKE_MEM_DEFINED(base, kRedZoneSize);
  assert(((size_t*)ptr)[0] == kMallocMagic);
  size = ((size_t*)ptr)[1];
  assert(((size_t*)ptr)[2] == kMallocMagic);
  /* Tell memcheck that this memory is poisoned now.
     Don't poison first 8 bytes as they are used by malloc/free internally. */
  VALGRIND_MAKE_MEM_NOACCESS(base + 2 * sizeof(size_t),
                             size - 2 * sizeof(size_t) + 2 * kRedZoneSize);
  VALGRIND_FREELIKE_BLOCK(base + kRedZoneSize, kRedZoneSize);
  /* Actually de-allocate a pointer free-ed some time ago
   (take it from the reuse queue) */
  old_ptr = delay_reuse_queue[drq_begin];
  CALL_FN_v_W(fn, old_ptr);
  /* Put the current pointer into the eruse queue. */
  delay_reuse_queue[drq_begin] = ptr;
  drq_begin = (drq_begin + 1) % DELAY_REUSE_QUEUE_SIZE;
}

/* malloc() */
size_t I_WRAP_SONAME_FNNAME_ZZ(NONE, malloc)(size_t size) {
  OrigFn fn;
  VALGRIND_GET_ORIG_FN(fn);
  return handle_malloc(fn, size);
}

/* free() */
void I_WRAP_SONAME_FNNAME_ZZ(NONE, free)(size_t ptr) {
  OrigFn fn;
  VALGRIND_GET_ORIG_FN(fn);
  handle_free(fn, ptr);
}

#endif  /* defined(__linux__) && defined(__x86_64__) */

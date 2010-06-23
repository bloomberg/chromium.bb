/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/* This file contains valgrind interceptors for NaCl's untrusted library
   functions such as malloc, free, etc.
   When running under valgrind, the function named foo() will be replaced
   with a function from this file named I_WRAP_SONAME_FNNAME_ZZ(NaCl, foo).
   The latter function may in turn call the original foo().
   This requires that valgrind assigns SONAME "NaCl" to the functions
   in untrusted code (i.e. an appropriate patch in valgrind).

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
#include <pthread.h>
#include <errno.h>
#include <sched.h>

#include "native_client/src/third_party/valgrind/nacl_valgrind.h"
#include "native_client/src/third_party/valgrind/nacl_memcheck.h"

/* For DYNAMIC_ANNOTATIONS_NAME() */
#include "native_client/src/untrusted/valgrind/dynamic_annotations.h"

#define INLINE __attribute__((always_inline))

#include "native_client/src/third_party/valgrind/ts_valgrind_client_requests.h"

/* For sizeof(nc_thread_memory_block_t) */
#include "native_client/src/untrusted/pthread/pthread_types.h"

/* This variable needs to be referenced by a program (either in sources,
  or using the linker flag -u) to which this library is linked.
  When using gcc/g++ as a linker, use -Wl,-u,have_nacl_valgrind_interceptors.
*/
int have_nacl_valgrind_interceptors;

/* TSan interceptors. */

#define VG_NACL_FUNC(f) I_WRAP_SONAME_FNNAME_ZZ(NaCl, f)
#define VG_NACL_ANN(f) \
  I_WRAP_SONAME_FNNAME_ZZ(NaCl, DYNAMIC_ANNOTATIONS_NAME(f))

#define VG_CREQ_v_W(_req, _arg1)                                        \
  do {                                                                  \
    uint64_t _res;                                                      \
    VALGRIND_DO_CLIENT_REQUEST(_res, 0, _req, _arg1, 0, 0, 0, 0);   \
  } while (0)

#define VG_CREQ_v_WW(_req, _arg1, _arg2)                                \
  do {                                                                  \
    uint64_t _res;                                                      \
    VALGRIND_DO_CLIENT_REQUEST(_res, 0, _req, _arg1, _arg2, 0, 0, 0);   \
  } while (0)

#define VG_CREQ_v_WWW(_req, _arg1, _arg2, _arg3)                        \
  do {                                                                  \
    uint64_t _res;                                                      \
    VALGRIND_DO_CLIENT_REQUEST(_res, 0, _req, _arg1, _arg2, _arg3, 0, 0); \
  } while (0)

static inline void start_ignore_all_accesses(void) {
  VG_CREQ_v_W(TSREQ_IGNORE_ALL_ACCESSES_BEGIN, 0);
}

static inline void stop_ignore_all_accesses(void) {
  VG_CREQ_v_W(TSREQ_IGNORE_ALL_ACCESSES_END, 0);
}

static inline void start_ignore_all_sync(void) {
  VG_CREQ_v_W(TSREQ_IGNORE_ALL_SYNC_BEGIN, 0);
}

static inline void stop_ignore_all_sync(void) {
  VG_CREQ_v_W(TSREQ_IGNORE_ALL_SYNC_END, 0);
}

static inline void start_ignore_all_accesses_and_sync(void) {
  start_ignore_all_accesses();
  start_ignore_all_sync();
}

static inline void stop_ignore_all_accesses_and_sync(void) {
  stop_ignore_all_accesses();
  stop_ignore_all_sync();
}


/*----------------------------------------------------------------*/
/*--- memory allocation                                        ---*/
/*----------------------------------------------------------------*/

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
enum {
  kDelayReuseQueueSize = 1024
};
static size_t delay_reuse_queue[kDelayReuseQueueSize];
static size_t drq_begin;

/* Generic malloc() handler. */
INLINE static size_t handle_malloc(OrigFn fn, size_t size) {
  size_t ptr;
  uint64_t base;
  start_ignore_all_accesses_and_sync();
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
  stop_ignore_all_accesses_and_sync();
  /* Tell TSan about malloc-ed memory. */
  VG_CREQ_v_WW(TSREQ_MALLOC, base + kRedZoneSize, size);
  /* Done */
  return ptr + kRedZoneSize;
}

/* Generic free() handler. */
INLINE static void handle_free(OrigFn fn, size_t ptr) {
  uint64_t base;
  size_t size, old_ptr;
  start_ignore_all_accesses_and_sync();
  /* Get the size of allocated region, check sanity. */
  ptr -= kRedZoneSize;
  base = VALGRIND_SANDBOX_PTR(ptr);
  /* Tell TSan about deallocated memory. */
  VG_CREQ_v_W(TSREQ_FREE, base + kRedZoneSize);
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
  drq_begin = (drq_begin + 1) % kDelayReuseQueueSize;
  stop_ignore_all_accesses_and_sync();
}

/* malloc() */
size_t VG_NACL_FUNC(malloc)(size_t size) {
  OrigFn fn;
  VALGRIND_GET_ORIG_FN(fn);
  return handle_malloc(fn, size);
}

/* free() */
void VG_NACL_FUNC(free)(size_t ptr) {
  OrigFn fn;
  VALGRIND_GET_ORIG_FN(fn);
  handle_free(fn, ptr);
}

/* nc_allocate_memory_block_mu() - a cached malloc for thread stack & tls. */
size_t VG_NACL_FUNC(nc_allocate_memory_block_mu)(int type, size_t size) {
  OrigFn fn;
  size_t ret;
  VALGRIND_GET_ORIG_FN(fn);
  start_ignore_all_accesses_and_sync();
  CALL_FN_W_WW(ret, fn, type, size);
  stop_ignore_all_accesses_and_sync();
  if (ret) {
    VG_CREQ_v_WW(TSREQ_MALLOC, VALGRIND_SANDBOX_PTR(ret),
        size + sizeof(nc_thread_memory_block_t));
  }
  return ret;
}

/* Tell the tool that the stack has moved.
   This is the first untrusted function of any NaCl thread and the first
   place after the context switch that we can intercept. */
void VG_NACL_FUNC(nc_thread_starter)(size_t func, size_t state) {
  OrigFn fn;
  int local_stack_var = 0;

  /* Let the tool guess where the stack starts. */
  VG_CREQ_v_W(TSREQ_THR_STACK_TOP, (size_t)&local_stack_var);

  VALGRIND_GET_ORIG_FN(fn);
  CALL_FN_v_WW(fn, func, state);
}


/*----------------------------------------------------------------*/
/* pthread_mutex_t functions */

/* pthread_mutex_init */
int VG_NACL_FUNC(pthreadZumutexZuinit)(pthread_mutex_t *mutex,
    pthread_mutexattr_t* attr) {
  int    ret;
  OrigFn fn;
  VALGRIND_GET_ORIG_FN(fn);

  CALL_FN_W_WW(ret, fn, (size_t)mutex, (size_t)attr);

  if (ret == 0 /*success*/) {
    VG_CREQ_v_WW(TSREQ_PTHREAD_RWLOCK_CREATE_POST,
        VALGRIND_SANDBOX_PTR((size_t)mutex), 0);
  }

  return ret;
}

/* pthread_mutex_destroy */
int VG_NACL_FUNC(pthreadZumutexZudestroy)(pthread_mutex_t *mutex) {
  int    ret;
  OrigFn fn;
  VALGRIND_GET_ORIG_FN(fn);

  VG_CREQ_v_W(TSREQ_PTHREAD_RWLOCK_DESTROY_PRE,
      VALGRIND_SANDBOX_PTR((size_t)mutex));

  CALL_FN_W_W(ret, fn, (size_t)mutex);

  return ret;
}

/* pthread_mutex_lock */
int VG_NACL_FUNC(pthreadZumutexZulock)(pthread_mutex_t *mutex) {
  int    ret;
  OrigFn fn;
  VALGRIND_GET_ORIG_FN(fn);

  start_ignore_all_accesses();
  CALL_FN_W_W(ret, fn, (size_t)mutex);
  stop_ignore_all_accesses();

  if (ret == 0 /*success*/) {
    VG_CREQ_v_WW(TSREQ_PTHREAD_RWLOCK_LOCK_POST,
        VALGRIND_SANDBOX_PTR((size_t)mutex), 1);
  }

  return ret;
}

/* pthread_mutex_trylock. */
int VG_NACL_FUNC(pthreadZumutexZutrylock)(pthread_mutex_t *mutex) {
  int    ret;
  OrigFn fn;
  VALGRIND_GET_ORIG_FN(fn);

  start_ignore_all_accesses();
  CALL_FN_W_W(ret, fn, (size_t)mutex);
  stop_ignore_all_accesses();

  if (ret == 0 /*success*/) {
    VG_CREQ_v_WW(TSREQ_PTHREAD_RWLOCK_LOCK_POST,
        VALGRIND_SANDBOX_PTR((size_t)mutex), 1);
  }

  return ret;
}

/* pthread_mutex_timedlock. */
int VG_NACL_FUNC(pthreadZumutexZutimedlock)(pthread_mutex_t *mutex,
    void* timeout) {
  int    ret;
  OrigFn fn;
  VALGRIND_GET_ORIG_FN(fn);

  start_ignore_all_accesses();
  CALL_FN_W_WW(ret, fn, (size_t)mutex, (size_t)timeout);
  stop_ignore_all_accesses();

  if (ret == 0 /*success*/) {
    VG_CREQ_v_WW(TSREQ_PTHREAD_RWLOCK_LOCK_POST,
        VALGRIND_SANDBOX_PTR((size_t)mutex), 1);
  }

  return ret;
}

/* pthread_mutex_unlock */
int VG_NACL_FUNC(pthreadZumutexZuunlock)(pthread_mutex_t *mutex) {
  int    ret;
  OrigFn fn;
  VALGRIND_GET_ORIG_FN(fn);

  VG_CREQ_v_W(TSREQ_PTHREAD_RWLOCK_UNLOCK_PRE,
      VALGRIND_SANDBOX_PTR((size_t)mutex));

  start_ignore_all_accesses();
  CALL_FN_W_W(ret, fn, (size_t)mutex);
  stop_ignore_all_accesses();

  return ret;
}

/* ------------------------------------------------------------------*/
/*             Limited support for dynamic annotations.              */

void VG_NACL_ANN(AnnotateTraceMemory)(char *file, int line, void *mem) {
  VG_CREQ_v_W(TSREQ_TRACE_MEM, VALGRIND_SANDBOX_PTR((size_t)mem));
}

void VG_NACL_ANN(AnnotateMutexIsNotPHB)(char *file, int line, void *mu) {
  VG_CREQ_v_W(TSREQ_MUTEX_IS_NOT_PHB, VALGRIND_SANDBOX_PTR((size_t)mu));
}

void VG_NACL_ANN(AnnotateExpectRace)(char *file, int line, void *addr,
    void* desc) {
  VG_CREQ_v_WWW(TSREQ_EXPECT_RACE, VALGRIND_SANDBOX_PTR((size_t)addr), 1,
      VALGRIND_SANDBOX_PTR((size_t)desc));
}

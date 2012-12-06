/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "native_client/src/untrusted/irt/irt.h"
#include "native_client/src/untrusted/irt/irt_private.h"
#include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"
#include "native_client/src/untrusted/nacl/tls.h"
#include "native_client/src/untrusted/nacl/tls_params.h"
#include "native_client/src/untrusted/pthread/futex.h"
#include "native_client/src/untrusted/pthread/pthread_internal.h"
#include "native_client/src/untrusted/pthread/pthread_types.h"


static struct nc_combined_tdb *get_irt_tdb(void *thread_ptr) {
  struct nc_combined_tdb *tdb = (void *) ((uintptr_t) thread_ptr +
                                          __nacl_tp_tdb_offset(sizeof(*tdb)));
  return tdb;
}

/*
 * This replaces __pthread_initialize_minimal() from libnacl and
 * __pthread_initialize() from libpthread.
 */
void __pthread_initialize(void) {
  struct nc_combined_tdb *tdb;

  __nc_futex_init();

  /*
   * Allocate the area.  If malloc fails here, we'll crash before it returns.
   */
  size_t combined_size = __nacl_tls_combined_size(sizeof(*tdb));
  void *combined_area = malloc(combined_size);

  /*
   * Initialize TLS proper (i.e., __thread variable initializers).
   */
  void *tp = __nacl_tls_initialize_memory(combined_area, sizeof(*tdb));
  tdb = get_irt_tdb(tp);
  __nc_initialize_unjoinable_thread(tdb);
  tdb->tdb.irt_thread_data = combined_area;

  /*
   * Now install it for later fetching.
   * This is what our private version of __nacl_read_tp will read.
   */
  NACL_SYSCALL(second_tls_set)(tp);

  /*
   * Finally, do newlib per-thread initialization.
   */
  __newlib_thread_init();

  __nc_initialize_globals();
}

static void nacl_irt_thread_exit(int32_t *stack_flag) {
  struct nc_combined_tdb *tdb = get_irt_tdb(NACL_SYSCALL(second_tls_get)());

  __nc_tsd_exit();
  __nc_futex_thread_exit();

  /*
   * Sanity check: Check that this function was not called on a thread
   * created by the IRT's internal pthread_create().  For such
   * threads, irt_thread_data == NULL.
   */
  assert(tdb->tdb.irt_thread_data != NULL);

  free(tdb->tdb.irt_thread_data);

  NACL_SYSCALL(thread_exit)(stack_flag);
  while (1) *(volatile int *) 0 = 0;  /* Crash.  */
}

/*
 * This is the real first entry point for new threads.
 */
static void irt_start_thread(void) {
  void *thread_ptr = NACL_SYSCALL(second_tls_get)();
  struct nc_combined_tdb *tdb = get_irt_tdb(thread_ptr);

  /*
   * Fetch the user's start routine.
   */
  void (*user_start)(void) = (void (*)(void)) tdb->tdb.start_func;

  /*
   * Now do per-thread initialization for the IRT-private C library state.
   */
  __newlib_thread_init();

  /*
   * Finally, run the user code.
   */
  (*user_start)();

  /*
   * That should never return.  Crash hard if it does.
   */
  while (1) *(volatile int *) 0 = 0;  /* Crash.  */
}

static int nacl_irt_thread_create(void *start_user_address, void *stack,
                                  void *thread_ptr) {
  struct nc_combined_tdb *tdb;

  /* @IGNORE_LINES_FOR_CODE_HYGIENE[1] */
#if defined(__i386__)
  /*
   * On x86-32, the user thread pointer is fetched by reading %gs:0.
   * The TCB may implement this in one of two ways:
   *
   *  (1) with the %gs segment pointing into untrusted address space,
   *      so that user code gets *(void **) thread_ptr; or
   *  (2) with the %gs segment pointing to a copy of the value
   *      thread_ptr in trusted address space.
   *
   * Even if we implement (2), we want to have the freedom to switch
   * back to (1) later, so we require the user thread library to set
   * up thread_ptr to point to itself.  This is not foolproof, because
   * user code might still modify *thread_ptr after the thread has
   * been launched.
   */
  if (*(void **) thread_ptr != thread_ptr)
    return EINVAL;
#endif

  /*
   * Before we start the thread, allocate the IRT-private TLS area for it.
   */
  size_t combined_size = __nacl_tls_combined_size(sizeof(*tdb));
  void *combined_area = malloc(combined_size);
  if (combined_area == NULL)
    return EAGAIN;

  /*
   * Note that __nacl_tls_initialize_memory() is not reversible,
   * because it takes a pointer that need not be aligned and can
   * return a pointer that is aligned.  In order to
   * free(combined_area) later, we must save the value of
   * combined_area.
   */
  void *irt_tp = __nacl_tls_initialize_memory(combined_area, sizeof(*tdb));
  tdb = get_irt_tdb(irt_tp);
  __nc_initialize_unjoinable_thread(tdb);
  tdb->tdb.irt_thread_data = combined_area;
  /*
   * We overload the libpthread start_func field to store a function
   * of a different type.
   */
  tdb->tdb.start_func = (void *(*)(void *)) (uintptr_t) start_user_address;

  int error = -NACL_SYSCALL(thread_create)(
      (void *) (uintptr_t) &irt_start_thread, stack, thread_ptr, irt_tp);
  if (error != 0)
    free(combined_area);
  return error;
}

static int nacl_irt_thread_nice(const int nice) {
  return -NACL_SYSCALL(thread_nice)(nice);
}

const struct nacl_irt_thread nacl_irt_thread = {
  nacl_irt_thread_create,
  nacl_irt_thread_exit,
  nacl_irt_thread_nice,
};

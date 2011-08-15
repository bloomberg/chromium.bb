/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>
#include <stdlib.h>

#include "native_client/src/untrusted/irt/irt.h"
#include "native_client/src/untrusted/irt/irt_private.h"
#include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"
#include "native_client/src/untrusted/nacl/tls.h"
#include "native_client/src/untrusted/nacl/tls_params.h"
#include "native_client/src/untrusted/pthread/pthread_internal.h"

/*
 * This remains zero until we complete __pthread_initialize, below.
 */
int irt_initialized;

/*
 * Each thread has an IRT-private TLS area.
 * This is the "thread descriptor" part of that.
 */
struct irt_private_tdb {
  /*
   * We use a union to reuse the same space for user_start initially,
   * and then for self after thread startup, keeping this struct as
   * small as possible.
   */
  union {
    void *self;
    void (*user_start)(void);
  };
  void *combined_area;
};

static struct irt_private_tdb *get_irt_tdb(void *thread_ptr) {
  struct irt_private_tdb *tdb = (void *) ((uintptr_t) thread_ptr +
                                          __nacl_tp_tdb_offset(sizeof(*tdb)));
  return tdb;
}

/*
 * This replaces __pthread_initialize_minimal from libnacl.
 * We set up our private TLS area with our irt_private_tdb structure.
 */
void __pthread_initialize(void) {
  struct irt_private_tdb *tdb;

  /*
   * Allocate the area.  If malloc fails here, we'll crash before it returns.
   */
  size_t combined_size = __nacl_tls_combined_size(sizeof(*tdb));
  void *combined_area = malloc(combined_size);

  /*
   * Initialize TLS proper (i.e., __thread variable initializers).
   */
  void *tp = __nacl_tls_initialize_memory(combined_area, sizeof(*tdb));

  /*
   * Set up our "private TDB".  Only the "self" pointer actually matters
   * here (and that only on some machines, like x86).  We'll never actually
   * free this combined_area, but no reason not to store the pointer.
   */
  tdb = get_irt_tdb(tp);
  tdb->combined_area = combined_area;
  tdb->self = tdb;

  /*
   * Now install it for later fetching.
   * This is what our private version of __nacl_read_tp will read.
   */
  NACL_SYSCALL(second_tls_set)(tp);

  /*
   * Finally, do newlib per-thread initialization.
   */
  __newlib_thread_init();

  /*
   * Mark that we are fully initialized, so malloc can fail gracefully.
   */
  irt_initialized = 1;
}

static void nacl_irt_thread_exit(int32_t *stack_flag) {
  struct irt_private_tdb *tdb = get_irt_tdb(NACL_SYSCALL(second_tls_get)());

  __nc_tsd_exit();

  free(tdb->combined_area);

  NACL_SYSCALL(thread_exit)(stack_flag);
  while (1) *(volatile int *) 0 = 0;  /* Crash.  */
}

/*
 * This is the real first entry point for new threads.
 */
static void irt_start_thread(void) {
  void *thread_ptr = NACL_SYSCALL(second_tls_get)();
  struct irt_private_tdb *tdb = get_irt_tdb(thread_ptr);

  /*
   * Fetch the user's start routine.
   * Then set the self pointer, which occupies the same word.
   */
  void (*user_start)(void) = tdb->user_start;
  tdb->self = thread_ptr;

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
  struct irt_private_tdb *tdb;

  /*
   * Before we start the thread, allocate the IRT-private TLS area for it.
   */
  size_t combined_size = __nacl_tls_combined_size(sizeof(*tdb));
  void *combined_area = malloc(combined_size);
  if (combined_area == NULL)
    return EAGAIN;

  void *irt_tp = __nacl_tls_initialize_memory(combined_area, sizeof(*tdb));
  tdb = get_irt_tdb(irt_tp);

  /*
   * Store the whole TLS area so we can free it at thread exit,
   * and the user's routine to be run.
   */
  tdb->combined_area = combined_area;
  tdb->user_start = (void (*)(void)) (uintptr_t) start_user_address;

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

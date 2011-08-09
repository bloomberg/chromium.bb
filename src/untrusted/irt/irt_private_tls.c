/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#include "native_client/src/untrusted/nacl/nacl_thread.h"
#include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"
#include "native_client/src/untrusted/nacl/tls.h"


/*
 * The integrated runtime (IRT) needs to have Thread Local Storage
 * isolated from the TLS of the user executable.
 *
 * This exists mainly:
 *  1) to make errno (and newlib's stdio) work inside the IRT without
 *     having to rebuild newlib;
 *  2) to make pthread_mutex_lock() work inside the IRT, since it
 *     expects to be able to get the current thread's ID.
 *
 * These functions override functions in libnacl (src/untrusted/nacl).
 */

int nacl_tls_init(void *thread_ptr) {
  return -NACL_SYSCALL(second_tls_set)(thread_ptr);
}

#define BAD_TLS ((void *) (intptr_t) -1)

void *nacl_tls_get() {
  void *value = NACL_SYSCALL(second_tls_get)();
  if (value == NULL) {
    /*
     * This thread has not been noticed by IRT code before.
     * Set up its second TLS area.
     *
     * First, we set it to the dummy pointer BAD_TLS so that we can
     * recognize a reentry from inside malloc.
     */
    NACL_SYSCALL(second_tls_set)(BAD_TLS);
    size_t combined_size = __nacl_tls_combined_size(sizeof(void *));
    void *combined_area = malloc(combined_size);
    /*
     * If malloc failed, it will have reentered here while trying to set errno.
     * That reaches the case below.
     */
    value = __nacl_tls_initialize_memory(combined_area, sizeof(void *));
    NACL_SYSCALL(second_tls_set)(value);
    __newlib_thread_init();
  } else if (value == BAD_TLS) {
    /*
     * We get here when the malloc above failed.
     * There really is no way to recover.
     */
    static const char msg[] = "IRT allocation failure in lazy thread setup!\n";
    write(2, msg, sizeof(msg) - 1);
    _exit(-1);
  }
  return value;
}

void *__nacl_read_tp() {
  return nacl_tls_get();
}

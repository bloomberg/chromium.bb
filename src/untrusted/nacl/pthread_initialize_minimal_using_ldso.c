/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <unistd.h>

#include "native_client/src/untrusted/nacl/nacl_irt.h"
#include "native_client/src/untrusted/nacl/nacl_thread.h"
#include "native_client/src/untrusted/nacl/tls.h"
#include "native_client/src/untrusted/nacl/tls_params.h"

/*
 * This initialization happens early in startup with or without libpthread.
 * It must make it safe for vanilla newlib code to run.
 */
int __pthread_initialize_minimal(size_t tdb_size) {
 /*
   * ld.so does most of the work for us, e.g.
   * allocating and initialzing the tls area.
   * For x86 we still need put the tdb in a
   * state which our libpthread expects.
   * This relies heavily on our tdb being "compatible" with
   * glibc's tdb.
   * C.f src/untrusted/pthread/pthread_types.h
   */

  /*
   * the extra initialization is only needed for x86
   */
  if (__nacl_tp_tdb_offset(tdb_size) == 0) {
    char *tp = __nacl_read_tp();
    void *tdb = (char *) tp + __nacl_tp_tdb_offset(tdb_size);
    *(void **) tdb = tdb;
  }

  /*
   * Initialize newlib's thread-specific pointer.
   */
  __newlib_thread_init();

  return 0;
}

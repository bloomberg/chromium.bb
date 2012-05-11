/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <unistd.h>

#include "native_client/src/untrusted/nacl/nacl_irt.h"
#include "native_client/src/untrusted/nacl/nacl_thread.h"
#include "native_client/src/untrusted/nacl/tls.h"

/*
 * This initialization happens early in startup with or without libpthread.
 * It must make it safe for vanilla newlib code to run.
 */
int __pthread_initialize_minimal(size_t tdb_size) {
  /* Adapt size for sbrk. */
  /* TODO(robertm): this is somewhat arbitrary - re-examine). */
  size_t combined_size = (__nacl_tls_combined_size(tdb_size) + 15) & ~15;

  /*
   * Use sbrk not malloc here since the library is not initialized yet.
   */
  void *combined_area = sbrk(combined_size);

  /*
   * Fill in that memory with its initializer data.
   */
  void *tp = __nacl_tls_initialize_memory(combined_area, tdb_size);

  /*
   * Set %gs, r9, or equivalent platform-specific mechanism.  Requires
   * a syscall since certain bitfields of these registers are trusted.
   */
  nacl_tls_init(tp);

  /*
   * Initialize newlib's thread-specific pointer.
   */
  __newlib_thread_init();

  return 0;
}

/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Native Client support for thread local storage
 */

#include <sys/unistd.h>
#include "native_client/src/untrusted/nacl/tls.h"
#include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"

/*
 * These stubs provide setup for thread local storage when libpthread is not
 * being used.  Since they are declared as weak symbols, they are overridden
 * if libpthread is used.
 */

/* @IGNORE_LINES_FOR_CODE_HYGIENE[2] */
extern int __pthread_initialize() __attribute__ ((weak));
extern int __pthread_shutdown() __attribute__ ((weak));

/* See tls.h. */
int __pthread_initialize_minimal(size_t tdb_size) {
  /* Use sbrk not malloc here since the library is not initialized yet. */
  size_t combined_size = __nacl_tls_combined_size(tdb_size);
  /* Adapt size for sbrk. */
  /* TODO(robertm): this is somewhat arbitrary - re-examine). */
  combined_size = (combined_size + 15) & ~15;
  void *combined_area = sbrk(combined_size);  /* (uses nacl_sysbrk) */

  __nacl_tls_data_bss_initialize_from_template(combined_area);

  /* Patch the first word of the TDB to contain the "base" of the TLS area. */
  void *tdb = __nacl_tls_tdb_start(combined_area);
  *(void**) tdb = tdb;

  /* Set %gs, r9, or equivalent platform-specific mechanism.  Requires
   * a syscall since certain bitfields of these registers are trusted. */
  NACL_SYSCALL(tls_init)(tdb, tdb_size);

  /* initialize newlib's thread-specific pointer. */
  __newlib_thread_init();
  return 0;
}

int __pthread_initialize() {
  /* all we need is to have the self pointer in the TDB */
  return __pthread_initialize_minimal(sizeof(void*));
}

int __pthread_shutdown() {
  /* No shutdown is required when pthread library is not used */
  return 0;
}

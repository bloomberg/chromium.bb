/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/untrusted/nacl/tls.h"

/*
 * These stubs provide setup for thread local storage when libpthread
 * is not being used.  Since libpthread is earlier in the link line,
 * its versions will be chosen instead and this file will not be
 * linked in.
 *
 * Declaring these weak is not really necessary for any sane linking
 * arrangement.  But it does no harm, and it is required by the silly
 * linking done in tests/toolchain/nacl.scons.
 */

__attribute__((weak))
int __pthread_initialize(void) {
  /*
   * All we need is to have the self pointer in the TDB.
   */
  return __pthread_initialize_minimal(sizeof(void *));
}

__attribute__((weak))
int __pthread_shutdown(void) {
  /*
   * No extra shutdown work is required when pthread library is not used.
   */
  __newlib_thread_exit();
  return 0;
}

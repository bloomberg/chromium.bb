/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/untrusted/nacl/nacl_irt.h"
#include "native_client/src/untrusted/nacl/nacl_thread.h"
#include "native_client/src/untrusted/nacl/tls.h"

/*
 * The compiler generates calls to __nacl_read_tp() for TLS accesses.
 * This is primarily used for x86-64.  See src/untrusted/nacl/tls.h.
 *
 * __nacl_read_tp() is also used on x86-32 when compiling with
 * "-mtls-use-call".  This is for when TLS accesses need to be
 * virtualised -- specifically, for object files that might get linked
 * into the integrated runtime (IRT) library.
 *
 * NOTE: In the glibc build, this is defined in ld.so rather than here.
 */
void *__nacl_read_tp(void) {
#if defined(__i386__)
  /*
   * Calling nacl_tls_get() works on x86-32, but reading %gs:0 is a
   * lot faster.
   */
  void *result;
  /* @IGNORE_LINES_FOR_CODE_HYGIENE[1] */
  __asm__("mov %%gs:0, %0" : "=r"(result));
  return result;
#else
  return nacl_tls_get();
#endif
}

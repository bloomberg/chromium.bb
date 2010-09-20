/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Native Client support for TLS, platform-specific components.
 * These routines are documented in the tls.h header included below.
 */

#include "native_client/src/untrusted/nacl/tls.h"
#include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"

/* See src/untrusted/nacl/tls.h */
int __nacl_tls_aligment() {
  return 32;
}


/* See src/untrusted/nacl/tls.h */
size_t __nacl_tdb_offset_in_tls(size_t tls_data_and_bss_size) {
  return tls_data_and_bss_size; /* TDB after TLS */
}


/* See src/untrusted/nacl/tls.h */
size_t __nacl_tdb_effective_payload_size(size_t tdb_size) {
  return tdb_size; /* TDB after TLS */
}


/* See src/untrusted/nacl/tls.h */
size_t __nacl_return_address_size() {
  return 8;
}


/* Compiler intrinsic: we only use this for x86-64.
 * See src/untrusted/nacl/tls.h */
void *__nacl_read_tp() {
  return NACL_SYSCALL(tls_get)();
}


/* Compiler intrinsic: we only use this for x86-64.
 * TODO(eaeltsin): get rid of this in favor of __nacl_read_tp.
 */
void *__tls_get_addr(int offset) {
  return ((char*) NACL_SYSCALL(tls_get)()) + offset;
}

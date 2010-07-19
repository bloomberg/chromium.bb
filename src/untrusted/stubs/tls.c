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
#if NACL_ARCH(NACL_TARGET_ARCH) == NACL_arm
  return (1 << 12);
#else
  return 32;
#endif
}


/* See src/untrusted/nacl/tls.h */
size_t __nacl_tdb_offset_in_tls(size_t tls_data_and_bss_size) {
#if NACL_ARCH(NACL_TARGET_ARCH) == NACL_arm
  return 0; /* TDB is the first symbol in the TLS. */
#else
  return tls_data_and_bss_size; /* TDB after TLS */
#endif
}


/* See src/untrusted/nacl/tls.h */
size_t __nacl_tdb_effective_payload_size(size_t tdb_size) {
#if NACL_ARCH(NACL_TARGET_ARCH) == NACL_arm
  return 0; /* TDB is the first symbol in the TLS. */
#else
  return tdb_size; /* TDB after TLS */
#endif
}


/* See src/untrusted/nacl/tls.h */
size_t __nacl_return_address_size() {
  /* We can't use sizeof(intptr_t) without relying stdint.h. */
#ifdef __x86_64__
  return 8;
#else /* __arm__ or __i386__ */
  return 4;
#endif
}


#ifdef __x86_64__
/* Compiler intrinsic. */
void *__tls_get_addr(int offset) {
  return ((char*) NACL_SYSCALL(tls_get)()) + offset;
}
#endif

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

/* See src/untrusted/nacl/tls.h */
int __nacl_tls_aligment() {
  return (1 << 12);
}


/* See src/untrusted/nacl/tls.h */
size_t __nacl_tdb_offset_in_tls(size_t tls_data_and_bss_size) {
  return 0; /* TDB is the first symbol in the TLS. */
}


/* See src/untrusted/nacl/tls.h */
size_t __nacl_tdb_effective_payload_size(size_t tdb_size) {
  return 0; /* TDB is the first symbol in the TLS. */
}


/* See src/untrusted/nacl/tls.h */
size_t __nacl_return_address_size() {
  return 4;
}

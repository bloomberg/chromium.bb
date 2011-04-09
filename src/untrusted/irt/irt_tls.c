/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/untrusted/nacl/nacl_thread.h"


/*
 * The integrated runtime (IRT) needs to have Thread Local Storage
 * isolated from the TLS of the user executable.
 *
 * This is a dummy implementation that is not thread-local.  This
 * exists mainly to make errno (and newlib's stdio) work inside the
 * IRT without having to rebuild newlib.
 *
 * These functions override functions in libnacl (src/untrusted/nacl).
 */

static void *dummy_tls;

int nacl_tls_init(void *tdb, size_t size) {
  dummy_tls = tdb;
  return 0;
}

void *nacl_tls_get() {
  return dummy_tls;
}

void *__nacl_read_tp() {
  return dummy_tls;
}

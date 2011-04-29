/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/untrusted/nacl/nacl_irt.h"
#include "native_client/src/untrusted/nacl/nacl_thread.h"

int nacl_thread_create(void *start_user_address, void *stack,
                       void *tdb, size_t tdb_size) {
  return __libnacl_irt_thread.thread_create(start_user_address, stack,
                                            tdb, tdb_size);
}

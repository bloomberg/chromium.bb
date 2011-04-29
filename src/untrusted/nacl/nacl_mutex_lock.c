/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/untrusted/nacl/nacl_irt.h"
#include "native_client/src/untrusted/nacl/nacl_thread.h"

int nacl_mutex_lock(int mutex_handle) {
  return __libnacl_irt_mutex.mutex_lock(mutex_handle);
}

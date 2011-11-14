/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/include/portability.h"
#include "native_client/src/trusted/service_runtime/sel_addrspace.h"

/*
 * Nothing to do here on Windows.
 */
void NaClAddrSpaceBeforeAlloc(size_t untrusted_size) {
  UNREFERENCED_PARAMETER(untrusted_size);
}

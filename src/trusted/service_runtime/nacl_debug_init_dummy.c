/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/service_runtime/nacl_debug_init.h"


/*
 * This stub implementation allows service_runtime code to be linked
 * without pulling in the debug stub implementation from
 * src/trusted/gdb_rsp and src/trusted/debug_stub.
 */
int NaClDebugInit(struct NaClApp *nap) {
  UNREFERENCED_PARAMETER(nap);
  return 1; /* Success */
}

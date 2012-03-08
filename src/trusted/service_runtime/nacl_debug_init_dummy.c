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
int NaClDebugInit(struct NaClApp *nap,
                  int argc, char const *const argv[],
                  int envc, char const *const envv[]) {
  UNREFERENCED_PARAMETER(nap);
  UNREFERENCED_PARAMETER(argc);
  UNREFERENCED_PARAMETER(argv);
  UNREFERENCED_PARAMETER(envc);
  UNREFERENCED_PARAMETER(envv);
  return 1; /* Success */
}

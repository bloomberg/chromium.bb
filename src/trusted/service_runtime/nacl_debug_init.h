/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_SERVICE_RUNTIME_NACL_DEBUG_INIT_H_
#define NATIVE_CLIENT_SRC_TRUSTED_SERVICE_RUNTIME_NACL_DEBUG_INIT_H_ 1

#include "native_client/src/trusted/service_runtime/sel_ldr.h"

EXTERN_C_BEGIN

/*
 * This may be called to tell the debug stub to bind the TCP port that
 * it listens on.  This may need to be called before enabling an outer
 * sandbox; otherwise, NaClDebugInit() calls it.  The function returns
 * whether it was successful.
 */
int NaClDebugBindSocket(void);

/*
 * Enables the debug stub.  If this is called, we do not guarantee
 * security to the same extent that we normally would.
 */
int NaClDebugInit(struct NaClApp *nap);

EXTERN_C_END

#endif

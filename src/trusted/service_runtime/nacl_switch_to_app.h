/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * NaCl Service Runtime
 */

#ifndef SERVICE_RUNTIME_NACL_SWITCH_TO_APP_H__
#define SERVICE_RUNTIME_NACL_SWITCH_TO_APP_H__ 1

#include "native_client/src/include/nacl_base.h"

EXTERN_C_BEGIN

struct NaClAppThread;

struct NaclThreadContext;

extern NORETURN void NaClSwitch(struct NaClThreadContext *context);

NORETURN void NaClStartThreadInApp(struct NaClAppThread *natp,
                                   uint32_t             new_prog_ctr);

NORETURN void NaClSwitchToApp(struct NaClAppThread *natp,
                              uint32_t             new_prog_ctr);

EXTERN_C_END

#endif

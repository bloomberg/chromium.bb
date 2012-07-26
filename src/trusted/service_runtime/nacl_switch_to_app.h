/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * NaCl Service Runtime
 */

#ifndef SERVICE_RUNTIME_NACL_SWITCH_TO_APP_H__
#define SERVICE_RUNTIME_NACL_SWITCH_TO_APP_H__ 1

#include "native_client/src/include/nacl_base.h"
#include "native_client/src/trusted/service_runtime/sel_rt.h"
/* get nacl_reg_t */

EXTERN_C_BEGIN

struct NaClAppThread;
struct NaClSignalContext;

void NaClInitSwitchToApp(struct NaClApp *nap);

#if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86
extern NORETURN void NaClSwitchAVX(struct NaClThreadContext *context);
extern NORETURN void NaClSwitchSSE(struct NaClThreadContext *context);
extern NORETURN void NaClSwitchNoSSE(struct NaClThreadContext *context);
extern NORETURN void NaClSwitchSavingStackPtr(
    struct NaClThreadContext *user_context,
    struct NaClThreadContext *sys_context,
    void (*NaClSwitch)(struct NaClThreadContext *context));
#if NACL_OSX
/* Same as NaClSwitchNoSSE but context in %ecx */
extern NORETURN void NaClSwitchNoSSEViaECX();
#endif
#else
extern NORETURN void NaClSwitch(struct NaClThreadContext *context);
#endif

NORETURN void NaClStartThreadInApp(struct NaClAppThread *natp,
                                   nacl_reg_t           new_prog_ctr);

NORETURN void NaClSwitchToApp(struct NaClAppThread *natp,
                              nacl_reg_t           new_prog_ctr);

NORETURN void NaClSwitchAllRegs(struct NaClAppThread     *natp,
                                struct NaClSignalContext *regs);

EXTERN_C_END

#endif

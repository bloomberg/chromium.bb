/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SERVICE_RUNTIME_NACL_SIGNAL_H__
#define NATIVE_CLIENT_SERVICE_RUNTIME_NACL_SIGNAL_H__ 1


#include "native_client/src/include/nacl_base.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"


/*
 * Allocates a stack suitable for passing to
 * NaClSignalStackRegister(), for use as a stack for signal handlers.
 * This can be called in any thread.
 * Stores the result in *result; returns 1 on success, 0 on failure.
 */
int NaClSignalStackAllocate(void **result);

/*
 * Deallocates a stack allocated by NaClSignalStackAllocate().
 * This can be called in any thread.
 */
void NaClSignalStackFree(void *stack);

/*
 * Registers a signal stack for use in the current thread.
 */
void NaClSignalStackRegister(void *stack);

/*
 * Undoes the effect of NaClSignalStackRegister().
 */
void NaClSignalStackUnregister(void);

/*
 * Register process-wide signal handlers.
 */
void NaClSignalHandlerInit(void);

/*
 * Undoes the effect of NaClSignalHandlerInit().
 */
void NaClSignalHandlerFini(void);

/*
 * Register NaClApp for use in the signal handler.
 */
void NaClSignalRegisterApp(struct NaClApp *nap);


#endif  /* NATIVE_CLIENT_SERVICE_RUNTIME_NACL_SIGNAL_H__ */

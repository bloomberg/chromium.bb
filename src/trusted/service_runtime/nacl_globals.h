/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * NaCl Server Runtime globals.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_SERVICE_RUNTIME_NACL_GLOBALS_H__
#define NATIVE_CLIENT_SRC_TRUSTED_SERVICE_RUNTIME_NACL_GLOBALS_H__

#include "native_client/src/include/portability.h"

EXTERN_C_BEGIN
struct NaClThreadContext;
struct NaClAppThread;
struct NaClMutex;
struct NaClApp;

#if NACL_WINDOWS
__declspec(dllexport)
/*
 * This array is exported so that it can be used by a debugger.  However, it is
 * not a stable interface and it may change or be removed in the future.  A
 * debugger using this interface could break.
 */
#endif
extern struct NaClThreadContext *nacl_user[];
extern struct NaClThreadContext *nacl_sys[];
#if NACL_WINDOWS
/*
 * NaCl Idx -> Thread ID mapping. Gdb scans this array to find NaCl index
 * by Thread ID.
 *
 * This is not a stable interface and it may change or be removed in
 * the future.  A debugger using this interface could break.
 */
__declspec(dllexport) extern uint32_t nacl_thread_ids[];
#endif
/*
 * nacl_user and nacl_sys are accessed w/o holding any locks.  once a
 * thread is live, only that thread itself may read/write the register
 * context contents (based on its %gs), and this allows a thread to
 * context switch from the application to the runtime, since we must
 * have a secure stack before calling any code, including lock
 * acquisition code.
 */

extern struct NaClMutex         nacl_thread_mu;
extern struct NaClAppThread     *nacl_thread[];
/*
 * nacl_thread_mu protects access to nacl_thread, which are
 * dynamically allocated (kernel stacks are too large to keep around
 * one for each potential thread).  once the pointer is accessed and
 * the object's lock held, nacl_thread_mu may be dropped.
 */

/*
 * TLS base used by nacl_tls_get.  User addresss.
 */
extern uint32_t                 nacl_tls[];

void  NaClGlobalModuleInit(void);
void  NaClGlobalModuleFini(void);

/* this is defined in src/trusted/service_runtime/arch/<arch>/ sel_rt.h */
void NaClInitGlobals(void);


/* hack for gdb */
#if NACL_WINDOWS
__declspec(dllexport)
#endif
extern uintptr_t nacl_global_xlate_base;

EXTERN_C_END

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_SERVICE_RUNTIME_NACL_GLOBALS_H__ */

/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * NaCl Server Runtime globals.
 */

#ifndef SERVICE_RUNTIME_NACL_GLOBALS_H__
#define SERVICE_RUNTIME_NACL_GLOBALS_H__  1

#include "native_client/src/include/portability.h"

struct NaClThreadContext;
struct NaClAppThread;
struct NaClMutex;
struct NaClApp;

extern struct NaClThreadContext *nacl_user[];
extern struct NaClThreadContext *nacl_sys[];
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

void  NaClGlobalModuleInit(void);
void  NaClGlobalModuleFini(void);

/* this is defined in src/trusted/service_runtime/arch/<arch>/ sel_rt.h */
void NaClInitGlobals(void);


/* hack for gdb */
extern uintptr_t nacl_global_xlate_base;

struct NaClApp        *GetCurProc(void);
struct NaClAppThread  *GetCurThread(void);

extern int NaClSrpcFileDescriptor;
#endif

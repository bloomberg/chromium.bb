/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * NaCl Server Runtime globals.
 */

#ifndef SERVICE_RUNTIME_NACL_GLOBALS_H__
#define SERVICE_RUNTIME_NACL_GLOBALS_H__  1

/* for LDT_ENTRIES */
#include "native_client/src/include/nacl_platform.h"

#include "native_client/src/trusted/platform/nacl_sync.h"
#include "native_client/src/trusted/service_runtime/nacl_config.h"
#include "native_client/src/trusted/service_runtime/nacl_app_thread.h"

extern struct NaClThreadContext *nacl_user[LDT_ENTRIES];
extern struct NaClThreadContext *nacl_sys[LDT_ENTRIES];
/*
 * nacl_user and nacl_sys are accessed w/o holding any locks.  once a
 * thread is live, only that thread itself may read/write the register
 * context contents (based on its %gs), and this allows a thread to
 * context switch from the application to the runtime, since we must
 * have a secure stack before calling any code, including lock
 * acquisition code.
 */

extern struct NaClMutex         nacl_thread_mu;
extern struct NaClAppThread     *nacl_thread[LDT_ENTRIES];
/*
 * nacl_thread_mu protects access to nacl_thread, which are
 * dynamically allocated (kernel stacks are too large to keep around
 * one for each potential thread).  once the pointer is accessed and
 * the object's lock held, nacl_thread_mu may be dropped.
 */

void  NaClGlobalModuleInit(void);
void  NaClGlobalModuleFini(void);

/* hack for gdb */
extern uintptr_t nacl_global_xlate_base;

struct NaClApp        *GetCurProc(void);
struct NaClAppThread  *GetCurThread(void);

extern int NaClSrpcFileDescriptor;
#endif

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
 * NaCl Server Runtime threads abstraction layer.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLATFORM_NACL_THREADS_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLATFORM_NACL_THREADS_H_

#include "native_client/src/include/nacl_base.h"
#include "native_client/src/include/portability.h"

EXTERN_C_BEGIN

#if NACL_LINUX || NACL_OSX
# include "native_client/src/shared/platform/linux/nacl_threads_types.h"
#elif NACL_WINDOWS
/*
 * Needed for WINAPI.
 */
# include "native_client/src/shared/platform/win/nacl_threads_types.h"
#else
# error "thread abstraction not defined for target OS"
#endif

typedef enum NaClThreadStatus {
  NACL_THREAD_OK,
  NACL_THREAD_RESOURCE_LIMIT
} NaClThreadStatus;

/*
 * Abstract low-level thread creation.  No thread attribute is
 * available, so no scheduling hint interface, no ability to set the
 * detach state, etc.  All threads are detached, and the main service
 * runtime thread must consult the NaClApp or NaClAppThread state to
 * figure out which thread(s) are still running.
 */
int NaClThreadCtor(struct NaClThread  *ntp,
                   void               (WINAPI *start_fn)(void *),
                   void               *state,
                   size_t             stack_size);

/*
 * NaClThreadDtor is used to clean up the NaClThread object, and
 * cannot be invoked while the thread is still alive.
 */
void NaClThreadDtor(struct NaClThread *ntp);

/*
 * NaClThreadExit is invoked by the thread itself.
 */
void NaClThreadExit(void);

/*
 * NaClThreadKill will be used to attempt to clean up after a badly
 * behaving NaClApp.
 */
void NaClThreadKill(struct NaClThread *target);

/*
 * Thread Specific Data.  Both Linux and Windows support Thread Local
 * Storage, but OSX does not.  Thus, we use the older, more primitive
 * TSD interface to get thread-specific information.
 */

int NaClTsdKeyCreate(struct NaClTsdKey  *tsdp);

int NaClTsdSetSpecific(struct NaClTsdKey  *tsdp,
                       void const         *ptr);

void *NaClTsdGetSpecific(struct NaClTsdKey  *tsdp);

uint32_t NaClThreadId(void);

EXTERN_C_END

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_PLATFORM_NACL_THREADS_H_ */

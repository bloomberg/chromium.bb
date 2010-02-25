/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * NaCl Server Runtime threads abstraction layer.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLATFORM_NACL_THREADS_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLATFORM_NACL_THREADS_H_

/*
 * We cannot include this header file from an installation that does not
 * have the native_client source tree.
 * TODO(sehr): use export_header.py to copy these files out.
 */
#include "native_client/src/include/nacl_base.h"
#include "native_client/src/include/portability.h"

EXTERN_C_BEGIN

#if NACL_LINUX || NACL_OSX || defined(__native_client__)
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

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
 * NaCl Server Runtime interruptible condvar, based on nacl_sync
 * interface. We need a condvar that can handle interruptible mutexes
 * correctly.
 */
#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLATFORM_NACL_INTERRUPTIBLE_CONDVAR_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLATFORM_NACL_INTERRUPTIBLE_CONDVAR_H_

#include "native_client/src/include/nacl_base.h"
#include "native_client/src/trusted/platform/nacl_interruptible_mutex.h"
#include "native_client/src/trusted/platform/nacl_sync.h"
#include "native_client/src/trusted/service_runtime/include/sys/time.h"


EXTERN_C_BEGIN


struct NaClIntrCondVar {
  struct NaClCondVar  cv;
};

int NaClIntrCondVarCtor(struct NaClIntrCondVar  *cp);

void NaClIntrCondVarDtor(struct NaClIntrCondVar *cp);

NaClSyncStatus NaClIntrCondVarWait(struct NaClIntrCondVar         *cp,
                                   struct NaClIntrMutex           *mp,
                                   struct nacl_abi_timespec const *ts);

NaClSyncStatus NaClIntrCondVarSignal(struct NaClIntrCondVar *cp);

NaClSyncStatus NaClIntrCondVarBroadcast(struct NaClIntrCondVar *cp);

void NaClIntrCondVarIntr(struct NaClIntrCondVar  *cp);

void NaClIntrCondVarReset(struct NaClIntrCondVar *cp);

EXTERN_C_END


#endif  /* NATIVE_CLIENT_SRC_TRUSTED_PLATFORM_NACL_INTERRUPTIBLE_CONDVAR_H_ */

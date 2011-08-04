/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * NaCl service runtime.  Bottom half (asynchronous device handling) code.
 */

#ifndef NATIVE_CLIENT_SERVICE_RUNTIME_NACL_BOTTOM_HALF_H_
#define NATIVE_CLIENT_SERVICE_RUNTIME_NACL_BOTTOM_HALF_H_

#include "native_client/src/include/portability.h"  /* uintptr_t */
#include "native_client/src/shared/platform/nacl_sync.h"

struct NaClAppThread;
struct NaClClosure;

struct NaClClosureResult {
  struct NaClMutex    mu;
  struct NaClCondVar  cv;
  int                 result_valid;
  void                *rv;
};

int NaClClosureResultCtor(struct NaClClosureResult *self) NACL_WUR;

void NaClClosureResultDtor(struct NaClClosureResult *self);

void *NaClClosureResultWait(struct NaClClosureResult *self);

void NaClClosureResultDone(struct NaClClosureResult *self,
                           void                     *rv);

void NaClStartAsyncOp(struct NaClAppThread  *natp,
                      struct NaClClosure    *ncp);

void *NaClWaitForAsyncOp(struct NaClAppThread *natp) NACL_WUR;

/*
 * Casts return value from NaClWaitForAsyncOp into something that
 * can be returned from a syscall
 */
int32_t NaClWaitForAsyncOpSysRet(struct NaClAppThread *natp) NACL_WUR;

#endif  /* NATIVE_CLIENT_SERVICE_RUNTIME_NACL_BOTTOM_HALF_H_ */

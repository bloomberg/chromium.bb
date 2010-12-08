/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * NaCl service runtime, bottom half routines for video.
 */

#include <string.h>
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_sync.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"

#include "native_client/src/trusted/service_runtime/nacl_app_thread.h"
#include "native_client/src/trusted/service_runtime/nacl_closure.h"
#include "native_client/src/trusted/service_runtime/nacl_sync_queue.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"

#include "native_client/src/trusted/service_runtime/include/sys/errno.h"

int NaClClosureResultCtor(struct NaClClosureResult *self) {
  self->result_valid = 0;
  self->rv = NULL;
  if (!NaClMutexCtor(&self->mu)) {
    return 0;
  }
  if (!NaClCondVarCtor(&self->cv)) {
    NaClMutexDtor(&self->mu);
    return 0;
  }
  return 1;
}

void NaClClosureResultDtor(struct NaClClosureResult *self) {
  NaClMutexDtor(&self->mu);
  NaClCondVarDtor(&self->cv);
}

void *NaClClosureResultWait(struct NaClClosureResult *self) {
  void  *rv;

  NaClXMutexLock(&self->mu);
  while (!self->result_valid) {
    NaClXCondVarWait(&self->cv, &self->mu);
  }
  rv = self->rv;
  self->result_valid = 0;
  NaClXMutexUnlock(&self->mu);

  return rv;
}

void NaClClosureResultDone(struct NaClClosureResult *self,
                           void                     *rv) {
  NaClXMutexLock(&self->mu);
  if (self->result_valid) {
    NaClLog(LOG_FATAL, "NaClClosureResultDone: result already present?!?\n");
  }
  self->rv = rv;
  self->result_valid = 1;
  NaClXCondVarSignal(&self->cv);
  NaClXMutexUnlock(&self->mu);
}

void NaClStartAsyncOp(struct NaClAppThread  *natp,
                      struct NaClClosure    *ncp) {
  NaClLog(4, "NaClStartAsyncOp(0x%08"NACL_PRIxPTR", 0x%08"NACL_PRIxPTR")\n",
          (uintptr_t) natp,
          (uintptr_t) ncp);
  NaClSyncQueueInsert(&natp->nap->work_queue, ncp);
  NaClLog(4, "Done\n");
}


void *NaClWaitForAsyncOp( struct NaClAppThread *natp ) {
  NaClLog(4, "NaClWaitForAsyncOp(0x%08"NACL_PRIxPTR")\n",
          (uintptr_t) natp);

  return NaClClosureResultWait(&natp->result);
}

int32_t NaClWaitForAsyncOpSysRet(struct NaClAppThread *natp) {
  uintptr_t result;
  int32_t result32;
  NaClLog(4, "NaClWaitForAsyncOp(0x%08"NACL_PRIxPTR")\n",
          (uintptr_t) natp);

  result = (uintptr_t) NaClClosureResultWait(&natp->result);
  result32 = (int32_t) result;

  if (result != (uintptr_t) result32) {
    NaClLog(LOG_ERROR,
            ("Overflow in NaClWaitForAsyncOpSysRet: return value is "
            "%"NACL_PRIxPTR"\n"),
            result);
    result32 = -NACL_ABI_EOVERFLOW;
  }

  return result32;
}


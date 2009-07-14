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
 * NaCl Service Runtime.  Semaphore Descriptor / Handle abstraction.
 */

#include "native_client/src/include/portability.h"

#include <stdlib.h>

#include "native_client/src/shared/imc/nacl_imc_c.h"

#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/desc/nacl_desc_mutex.h"

#include "native_client/src/trusted/platform/nacl_log.h"
#include "native_client/src/trusted/platform/nacl_semaphore.h"

#include "native_client/src/trusted/service_runtime/internal_errno.h"
#include "native_client/src/trusted/service_runtime/nacl_config.h"
#include "native_client/src/trusted/service_runtime/include/sys/errno.h"
#include "native_client/src/trusted/service_runtime/include/sys/fcntl.h"
#include "native_client/src/trusted/service_runtime/include/sys/mman.h"


/*
 * This file contains the implementation for the NaClDescSemaphore subclass
 * of NaClDesc.
 *
 * NaClDescSemaphore is the subclass that wraps host-OS semaphore abstractions
 */

int NaClDescSemaphoreCtor(struct NaClDescSemaphore  *self, int value) {
  struct NaClDesc *basep = (struct NaClDesc *) self;

  basep->vtbl = (struct NaClDescVtbl *) NULL;
  if (!NaClDescCtor(basep)) {
    return 0;
  }
  if (!NaClSemCtor(&self->sem, value)) {
    NaClDescDtor(basep);
    return 0;
  }

  basep->vtbl = &kNaClDescSemaphoreVtbl;
  return 1;
}

void NaClDescSemaphoreDtor(struct NaClDesc *vself) {
  struct NaClDescSemaphore *self = (struct NaClDescSemaphore *) vself;

  NaClLog(4, "NaClDescSemaphoreDtor(0x%08"PRIxPTR").\n",
          (uintptr_t) vself);
  NaClSemDtor(&self->sem);
  vself->vtbl = (struct NaClDescVtbl *) NULL;
  NaClDescDtor(&self->base);
}

int NaClDescSemaphoreClose(struct NaClDesc          *vself,
                           struct NaClDescEffector  *effp) {
  NaClDescUnref(vself);
  return 0;
}

int NaClDescSemaphorePost(struct NaClDesc         *vself,
                          struct NaClDescEffector *effp) {
  struct NaClDescSemaphore *self = (struct NaClDescSemaphore *)vself;

  NaClSyncStatus status = NaClSemPost(&self->sem);
  return -NaClXlateNaClSyncStatus(status);
}

int NaClDescSemaphoreSemWait(struct NaClDesc          *vself,
                             struct NaClDescEffector  *effp) {
  struct NaClDescSemaphore *self = (struct NaClDescSemaphore *)vself;

  NaClSyncStatus status = NaClSemWait(&self->sem);
  return -NaClXlateNaClSyncStatus(status);
}

int NaClDescSemaphoreGetValue(struct NaClDesc         *vself,
                              struct NaClDescEffector *effp) {
  NaClLog(LOG_ERROR, "SemGetValue is not implemented yet\n");
  return -NACL_ABI_EINVAL;
  /*
   * TODO(gregoryd): sem_getvalue is not implemented on OSX.
   * Remove this syscall or implement it using semctl
   */
#if 0
  struct NaClDescSemaphore *self = (struct NaClDescSemaphore *) vself;
  NaClSyncStatus status = NaClSemGetValue(&self->sem);
  return NaClXlateNaClSyncStatus(status);
#endif
}


struct NaClDescVtbl const kNaClDescSemaphoreVtbl = {
  NaClDescSemaphoreDtor,
  NaClDescMapNotImplemented,
  NaClDescUnmapUnsafeNotImplemented,
  NaClDescUnmapNotImplemented,
  NaClDescReadNotImplemented,
  NaClDescWriteNotImplemented,
  NaClDescSeekNotImplemented,
  NaClDescIoctlNotImplemented,
  NaClDescFstatNotImplemented,
  NaClDescSemaphoreClose,
  NaClDescGetdentsNotImplemented,
  NACL_DESC_SEMAPHORE,
  NaClDescExternalizeSizeNotImplemented,
  NaClDescExternalizeNotImplemented,
  NaClDescLockNotImplemented,
  NaClDescTryLockNotImplemented,
  NaClDescUnlockNotImplemented,
  NaClDescWaitNotImplemented,
  NaClDescTimedWaitAbsNotImplemented,
  NaClDescSignalNotImplemented,
  NaClDescBroadcastNotImplemented,
  NaClDescSendMsgNotImplemented,
  NaClDescRecvMsgNotImplemented,
  NaClDescConnectAddrNotImplemented,
  NaClDescAcceptConnNotImplemented,
  NaClDescSemaphorePost,
  NaClDescSemaphoreSemWait,
  NaClDescSemaphoreGetValue,
};

int NaClDescSemaphoreInternalize(struct NaClDesc           **baseptr,
                                 struct NaClDescXferState  *xfer) {
  NaClLog(LOG_ERROR, "NaClDescSemaphoreInternalize: not shared yet\n");
  return -NACL_ABI_EINVAL;
}

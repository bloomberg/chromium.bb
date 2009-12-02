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
 * NaCl Service Runtime.  Condition Variable Descriptor / Handle abstraction.
 */

#include "native_client/src/include/portability.h"

#include <stdlib.h>
#include <string.h>

#include "native_client/src/shared/imc/nacl_imc_c.h"
#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/desc/nacl_desc_mutex.h"

#include "native_client/src/shared/platform/nacl_host_desc.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/service_runtime/nacl_config.h"

#include "native_client/src/trusted/service_runtime/include/sys/errno.h"
#include "native_client/src/trusted/service_runtime/include/sys/fcntl.h"
#include "native_client/src/trusted/service_runtime/include/sys/mman.h"
#include "native_client/src/trusted/service_runtime/include/sys/stat.h"

/*
 * This file contains the implementation for the NaClDescCondVar subclass
 * of NaClDesc.
 *
 * NaClDescCondVar is the subclass that wraps host-OS condition
 * variable abstractions
 */

int NaClDescCondVarCtor(struct NaClDescCondVar  *self) {
  struct NaClDesc *basep = (struct NaClDesc *) self;

  basep->vtbl = (struct NaClDescVtbl *) NULL;
  if (!NaClDescCtor(basep)) {
    return 0;
  }
  if (!NaClIntrCondVarCtor(&self->cv)) {
    NaClDescDtor(basep);
    return 0;
  }

  basep->vtbl = &kNaClDescCondVarVtbl;
  return 1;
}

void NaClDescCondVarDtor(struct NaClDesc *vself) {
  struct NaClDescCondVar *self = (struct NaClDescCondVar *) vself;

  NaClLog(4, "NaClDescCondVarDtor(0x%08"PRIxPTR").\n",
          (uintptr_t) vself);
  NaClIntrCondVarDtor(&self->cv);

  vself->vtbl = (struct NaClDescVtbl *) NULL;
  NaClDescDtor(&self->base);
}

int NaClDescCondVarFstat(struct NaClDesc          *vself,
                         struct NaClDescEffector  *effp,
                         struct nacl_abi_stat     *statbuf) {
  UNREFERENCED_PARAMETER(vself);
  UNREFERENCED_PARAMETER(effp);

  memset(statbuf, 0, sizeof *statbuf);
  statbuf->nacl_abi_st_mode = NACL_ABI_S_IFCOND | NACL_ABI_S_IRWXU;
  return 0;
}

int NaClDescCondVarClose(struct NaClDesc          *vself,
                         struct NaClDescEffector  *effp) {
  UNREFERENCED_PARAMETER(effp);

  NaClDescUnref(vself);
  return 0;
}



int NaClDescCondVarWait(struct NaClDesc         *vself,
                        struct NaClDescEffector *effp,
                        struct NaClDesc         *mutex) {
  struct NaClDescCondVar  *self = (struct NaClDescCondVar *) vself;
  struct NaClDescMutex    *mutex_desc;
  NaClSyncStatus          status;

  UNREFERENCED_PARAMETER(effp);

  if (mutex->vtbl->typeTag != NACL_DESC_MUTEX) {
    return -NACL_ABI_EINVAL;
  }
  mutex_desc = (struct NaClDescMutex *)mutex;
  status = NaClIntrCondVarWait(&self->cv,
                               &mutex_desc->mu,
                               NULL);
  return -NaClXlateNaClSyncStatus(status);
}

int NaClDescCondVarTimedWaitAbs(struct NaClDesc                *vself,
                                struct NaClDescEffector        *effp,
                                struct NaClDesc                *mutex,
                                struct nacl_abi_timespec const *ts) {
  struct NaClDescCondVar  *self = (struct NaClDescCondVar *) vself;
  struct NaClDescMutex    *mutex_desc;
  NaClSyncStatus          status;

  UNREFERENCED_PARAMETER(effp);

  if (mutex->vtbl->typeTag != NACL_DESC_MUTEX) {
    return -NACL_ABI_EINVAL;
  }
  mutex_desc = (struct NaClDescMutex *) mutex;

  status = NaClIntrCondVarWait(&self->cv,
                               &mutex_desc->mu,
                               ts);
  return -NaClXlateNaClSyncStatus(status);
}

int NaClDescCondVarSignal(struct NaClDesc         *vself,
                          struct NaClDescEffector *effp) {
  struct NaClDescCondVar *self = (struct NaClDescCondVar *) vself;
  NaClSyncStatus status = NaClIntrCondVarSignal(&self->cv);

  UNREFERENCED_PARAMETER(effp);
  return -NaClXlateNaClSyncStatus(status);
}

int NaClDescCondVarBroadcast(struct NaClDesc          *vself,
                             struct NaClDescEffector  *effp) {
  struct NaClDescCondVar *self = (struct NaClDescCondVar *) vself;
  NaClSyncStatus status = NaClIntrCondVarBroadcast(&self->cv);

  UNREFERENCED_PARAMETER(effp);
  return -NaClXlateNaClSyncStatus(status);
}

struct NaClDescVtbl const kNaClDescCondVarVtbl = {
  NaClDescCondVarDtor,
  NaClDescMapNotImplemented,
  NaClDescUnmapUnsafeNotImplemented,
  NaClDescUnmapNotImplemented,
  NaClDescReadNotImplemented,
  NaClDescWriteNotImplemented,
  NaClDescSeekNotImplemented,
  NaClDescIoctlNotImplemented,
  NaClDescCondVarFstat,
  NaClDescCondVarClose,
  NaClDescGetdentsNotImplemented,
  NACL_DESC_CONDVAR,
  NaClDescExternalizeSizeNotImplemented,
  NaClDescExternalizeNotImplemented,
  NaClDescLockNotImplemented,
  NaClDescTryLockNotImplemented,
  NaClDescUnlockNotImplemented,
  NaClDescCondVarWait,
  NaClDescCondVarTimedWaitAbs,
  NaClDescCondVarSignal,
  NaClDescCondVarBroadcast,
  NaClDescSendMsgNotImplemented,
  NaClDescRecvMsgNotImplemented,
  NaClDescConnectAddrNotImplemented,
  NaClDescAcceptConnNotImplemented,
  NaClDescPostNotImplemented,
  NaClDescSemWaitNotImplemented,
  NaClDescGetValueNotImplemented,
};

int NaClDescCondVarInternalize(struct NaClDesc           **baseptr,
                               struct NaClDescXferState  *xfer) {
  UNREFERENCED_PARAMETER(baseptr);
  UNREFERENCED_PARAMETER(xfer);

  NaClLog(LOG_ERROR, "NaClDescCondVarInternalize: not shared yet\n");
  return -NACL_ABI_EINVAL;
}

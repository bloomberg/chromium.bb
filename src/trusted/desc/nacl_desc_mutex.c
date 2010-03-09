/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * NaCl Service Runtime.  Mutex Descriptor / Handle abstraction.
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
#include "native_client/src/trusted/service_runtime/internal_errno.h"

#include "native_client/src/trusted/service_runtime/include/sys/errno.h"
#include "native_client/src/trusted/service_runtime/include/sys/fcntl.h"
#include "native_client/src/trusted/service_runtime/include/sys/mman.h"
#include "native_client/src/trusted/service_runtime/include/sys/stat.h"

/*
 * This file contains the implementation for the NaClDescMutex subclass
 * of NaClDesc.
 *
 * NaClDescMutex is the subclass that wraps host-OS mutex abstractions
 */

/*
 * Takes ownership of hd, will close in Dtor.
 */
int NaClDescMutexCtor(struct NaClDescMutex  *self) {
  struct NaClDesc *basep = (struct NaClDesc *) self;

  basep->vtbl = (struct NaClDescVtbl *) NULL;
  if (!NaClDescCtor(basep)) {
    return 0;
  }
  if (!NaClIntrMutexCtor(&self->mu)) {
    NaClDescDtor(basep);
    return 0;
  }

  basep->vtbl = &kNaClDescMutexVtbl;
  return 1;
}

void NaClDescMutexDtor(struct NaClDesc *vself) {
  struct NaClDescMutex *self = (struct NaClDescMutex *) vself;

  NaClLog(4, "NaClDescMutexDtor(0x%08"NACL_PRIxPTR").\n",
          (uintptr_t) vself);
  NaClIntrMutexDtor(&self->mu);
  vself->vtbl = (struct NaClDescVtbl *) NULL;
  NaClDescDtor(&self->base);
}

int NaClDescMutexFstat(struct NaClDesc          *vself,
                       struct NaClDescEffector  *effp,
                       struct nacl_abi_stat     *statbuf) {
  UNREFERENCED_PARAMETER(vself);
  UNREFERENCED_PARAMETER(effp);

  memset(statbuf, 0, sizeof *statbuf);
  statbuf->nacl_abi_st_mode = NACL_ABI_S_IFMUTEX;
  return 0;
}

int NaClDescMutexClose(struct NaClDesc          *vself,
                       struct NaClDescEffector  *effp) {
  UNREFERENCED_PARAMETER(effp);

  NaClDescUnref(vself);
  return 0;
}

int NaClDescMutexLock(struct NaClDesc         *vself,
                      struct NaClDescEffector *effp) {
  struct NaClDescMutex *self = (struct NaClDescMutex *) vself;

  NaClSyncStatus status = NaClIntrMutexLock(&self->mu);

  UNREFERENCED_PARAMETER(effp);
  return -NaClXlateNaClSyncStatus(status);
}

int NaClDescMutexTryLock(struct NaClDesc          *vself,
                         struct NaClDescEffector  *effp) {
  struct NaClDescMutex *self = (struct NaClDescMutex *) vself;

  NaClSyncStatus status = NaClIntrMutexTryLock(&self->mu);

  UNREFERENCED_PARAMETER(effp);

  return -NaClXlateNaClSyncStatus(status);
}

int NaClDescMutexUnlock(struct NaClDesc         *vself,
                        struct NaClDescEffector *effp) {
  struct NaClDescMutex *self = (struct NaClDescMutex *) vself;

  NaClSyncStatus status = NaClIntrMutexUnlock(&self->mu);

  UNREFERENCED_PARAMETER(effp);

  return -NaClXlateNaClSyncStatus(status);
}

struct NaClDescVtbl const kNaClDescMutexVtbl = {
  NaClDescMutexDtor,
  NaClDescMapNotImplemented,
  NaClDescUnmapUnsafeNotImplemented,
  NaClDescUnmapNotImplemented,
  NaClDescReadNotImplemented,
  NaClDescWriteNotImplemented,
  NaClDescSeekNotImplemented,
  NaClDescIoctlNotImplemented,
  NaClDescMutexFstat,
  NaClDescMutexClose,
  NaClDescGetdentsNotImplemented,
  NACL_DESC_MUTEX,
  NaClDescExternalizeSizeNotImplemented,
  NaClDescExternalizeNotImplemented,
  NaClDescMutexLock,
  NaClDescMutexTryLock,
  NaClDescMutexUnlock,
  NaClDescWaitNotImplemented,
  NaClDescTimedWaitAbsNotImplemented,
  NaClDescSignalNotImplemented,
  NaClDescBroadcastNotImplemented,
  NaClDescSendMsgNotImplemented,
  NaClDescRecvMsgNotImplemented,
  NaClDescConnectAddrNotImplemented,
  NaClDescAcceptConnNotImplemented,
  NaClDescPostNotImplemented,
  NaClDescSemWaitNotImplemented,
  NaClDescGetValueNotImplemented,
};

int NaClDescMutexInternalize(struct NaClDesc           **baseptr,
                             struct NaClDescXferState  *xfer) {
  UNREFERENCED_PARAMETER(baseptr);
  UNREFERENCED_PARAMETER(xfer);

  NaClLog(LOG_ERROR, "NaClDescMutexInternalize: not shared yet\n");
  return -NACL_ABI_EINVAL;
}

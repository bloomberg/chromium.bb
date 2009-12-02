/*
 * Copyright (c) 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * NaCl Service Runtime.  I/O Descriptor / Handle abstraction.
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "native_client/src/include/portability.h"

#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/desc/nacl_desc_imc.h"

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_sync.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"

#if NACL_WINDOWS
# include "native_client/src/shared/platform/win/xlate_system_error.h"
#endif

#include "native_client/src/trusted/service_runtime/internal_errno.h"
#include "native_client/src/trusted/service_runtime/nacl_app_thread.h"
#include "native_client/src/trusted/service_runtime/nacl_config.h"
#include "native_client/src/trusted/service_runtime/sel_util.h"
#include "native_client/src/trusted/service_runtime/sel_mem.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"

#include "native_client/src/trusted/service_runtime/include/sys/errno.h"
#include "native_client/src/trusted/service_runtime/include/sys/stat.h"

/*
 * This file contains the implementation of the NaClDescSyncSocket
 * subclass of NaClDesc.
 *
 * NaClDescSyncSocket is the subclass that wraps base::SyncSocket descriptors.
 */

int NaClDescSyncSocketCtor(struct NaClDescSyncSocket  *self,
                           NaClHandle                 h) {
  int retval;
  retval = NaClDescCtor(&self->base);
  if (!retval) {
    return 0;
  }

  self->h = h;
  self->base.vtbl = &kNaClDescSyncSocketVtbl;

  return retval;
}

void NaClDescSyncSocketDtor(struct NaClDesc *vself) {
  struct NaClDescSyncSocket  *self = ((struct NaClDescSyncSocket *) vself);

  NaClClose(self->h);
  self->h = NACL_INVALID_HANDLE;
  self->base.vtbl = (struct NaClDescVtbl *) NULL;
  NaClDescDtor(vself);
}

int NaClDescSyncSocketFstat(struct NaClDesc          *vself,
                            struct NaClDescEffector  *effp,
                            struct nacl_abi_stat     *statbuf) {
  UNREFERENCED_PARAMETER(vself);
  UNREFERENCED_PARAMETER(effp);

  memset(statbuf, 0, sizeof *statbuf);
  statbuf->nacl_abi_st_mode = NACL_ABI_S_IFDSOCK;
  return 0;
}

int NaClDescSyncSocketClose(struct NaClDesc          *vself,
                            struct NaClDescEffector  *effp) {
  UNREFERENCED_PARAMETER(effp);

  NaClDescUnref(vself);
  return 0;
}

ssize_t NaClDescSyncSocketRead(struct NaClDesc          *vself,
                               struct NaClDescEffector  *effp,
                               void                     *buf,
                               size_t                   len) {
  struct NaClDescSyncSocket *self = (struct NaClDescSyncSocket *) vself;

  UNREFERENCED_PARAMETER(effp);

  return NaClDescReadFromHandle(self->h, buf, len);
}

ssize_t NaClDescSyncSocketWrite(struct NaClDesc         *vself,
                                struct NaClDescEffector *effp,
                                void const              *buf,
                                size_t                  len) {
  struct NaClDescSyncSocket *self = (struct NaClDescSyncSocket *) vself;

  UNREFERENCED_PARAMETER(effp);

  return NaClDescWriteToHandle(self->h, buf, len);
}


int NaClDescSyncSocketExternalizeSize(struct NaClDesc  *vself,
                                      size_t           *nbytes,
                                      size_t           *nhandles) {
  UNREFERENCED_PARAMETER(vself);
  NaClLog(4, "Entered NaClDescSyncSocketExternalizeSize\n");
  *nbytes = 0;
  *nhandles = 1;

  return 0;
}

int NaClDescSyncSocketExternalize(struct NaClDesc          *vself,
                                  struct NaClDescXferState *xfer) {
  struct NaClDescSyncSocket *self = ((struct NaClDescSyncSocket *) vself);

  NaClLog(4, "Entered NaClDescSyncSocketExternalize\n");
  *xfer->next_handle++ = self->h;
  return 0;
}


struct NaClDescVtbl const kNaClDescSyncSocketVtbl = {
  NaClDescSyncSocketDtor,
  NaClDescMapNotImplemented,
  NaClDescUnmapUnsafeNotImplemented,
  NaClDescUnmapNotImplemented,
  NaClDescSyncSocketRead,
  NaClDescSyncSocketWrite,
  NaClDescSeekNotImplemented,
  NaClDescIoctlNotImplemented,
  NaClDescSyncSocketFstat,
  NaClDescSyncSocketClose,
  NaClDescGetdentsNotImplemented,
  NACL_DESC_SYNC_SOCKET,
  NaClDescSyncSocketExternalizeSize,
  NaClDescSyncSocketExternalize,
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
  NaClDescPostNotImplemented,
  NaClDescSemWaitNotImplemented,
  NaClDescGetValueNotImplemented,
};


int NaClDescSyncSocketInternalize(struct NaClDesc          **baseptr,
                                  struct NaClDescXferState *xfer) {
  int                       rv;
  NaClHandle                h;
  struct NaClDescSyncSocket *ndssp;

  NaClLog(4, "Entered NaClDescSyncSocketInternalize\n");
  rv = -NACL_ABI_EIO;
  h = NACL_INVALID_HANDLE;
  ndssp = NULL;

  if (xfer->next_handle == xfer->handle_buffer_end) {
    NaClLog(LOG_ERROR,
            ("NaClSyncSocketInternalize: no descriptor"
             " left in xfer state\n"));
    rv = -NACL_ABI_EIO;
    goto cleanup;
  }
  ndssp = malloc(sizeof *ndssp);
  if (NULL == ndssp) {
    NaClLog(LOG_ERROR,
            "NaClSyncSocketInternalize: no memory\n");
    rv = -NACL_ABI_ENOMEM;
    goto cleanup;
  }
  if (!NaClDescSyncSocketCtor(ndssp, *xfer->next_handle)) {
    NaClLog(LOG_ERROR,
            "NaClSyncSocketInternalize: descriptor ctor error\n");
    rv = -NACL_ABI_EIO;
    goto cleanup;
  }
  *xfer->next_handle++ = NACL_INVALID_HANDLE;
  *baseptr = (struct NaClDesc *) ndssp;
  rv = 0;

cleanup:
  if (rv < 0) {
    free(ndssp);
  }
  return rv;
}

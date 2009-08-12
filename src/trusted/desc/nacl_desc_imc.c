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
 * NaCl Service Runtime.  I/O Descriptor / Handle abstraction.
 */

#include <stdlib.h>
#include <errno.h>

#include "native_client/src/include/portability.h"

#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/desc/nacl_desc_imc.h"

#include "native_client/src/shared/platform/nacl_log.h"
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

/*
 * This file contains the implementation of the NaClDescImcDesc
 * subclass of NaClDesc.
 *
 * NaClDescImcDesc is the subclass that wraps IMC socket descriptors.
 */

int NaClDescImcDescCtor(struct NaClDescImcDesc  *self,
                        NaClHandle              h) {
  struct NaClDesc *basep = (struct NaClDesc *) self;

  basep->vtbl = (struct NaClDescVtbl *) NULL;
  if (!NaClDescCtor(basep)) {
    return 0;
  }
  self->h = h;
  basep->vtbl = &kNaClDescImcDescVtbl;
  return 1;
}

void NaClDescImcDescDtor(struct NaClDesc *vself) {
  struct NaClDescImcDesc  *self = (struct NaClDescImcDesc *) vself;

  NaClClose(self->h);
  self->h = NACL_INVALID_HANDLE;
  vself->vtbl = (struct NaClDescVtbl *) NULL;
  NaClDescDtor(vself);
}

int NaClDescImcDescClose(struct NaClDesc          *vself,
                         struct NaClDescEffector  *effp) {
  NaClDescUnref(vself);
  return 0;
}

int NaClDescImcDescExternalizeSize(struct NaClDesc  *vself,
                                   size_t           *nbytes,
                                   size_t           *nhandles) {
  *nbytes = 0;
  *nhandles = 1;

  return 0;
}

int NaClDescImcDescExternalize(struct NaClDesc          *vself,
                               struct NaClDescXferState *xfer) {
  struct NaClDescImcDesc  *self = (struct NaClDescImcDesc *) vself;

  *xfer->next_handle++ = self->h;
  return 0;
}

/* we could expose sendmsg to descriptors, but only on unix-like OSes */
int NaClDescImcDescSendMsg(struct NaClDesc          *vself,
                           struct NaClDescEffector  *effp,
                           struct NaClMessageHeader *dgram,
                           int                      flags) {
  struct NaClDescImcDesc *self = (struct NaClDescImcDesc *) vself;

  int result = NaClSendDatagram(self->h, dgram, flags);
  if (-1 == result) {
#if NACL_WINDOWS
    return -NaClXlateSystemError(GetLastError());
#elif NACL_LINUX || NACL_OSX
    return -errno;
#else
# error "Unknown target platform: cannot translate error code(s) from SendMsg"
#endif
  }
  return result;
}

int NaClDescImcDescRecvMsg(struct NaClDesc          *vself,
                           struct NaClDescEffector  *effp,
                           struct NaClMessageHeader *dgram,
                           int                      flags) {
  struct NaClDescImcDesc  *self = (struct NaClDescImcDesc *) vself;

  int result = NaClReceiveDatagram(self->h, dgram, flags);
  if (-1 == result) {
#if NACL_WINDOWS
    return -NaClXlateSystemError(GetLastError());
#elif NACL_LINUX || NACL_OSX
    return -errno;
#else
# error "Unknown target platform: cannot translate error code(s) from RecvMsg"
#endif
  }
  return result;
}

struct NaClDescVtbl const kNaClDescImcDescVtbl = {
  NaClDescImcDescDtor,
  NaClDescMapNotImplemented,
  NaClDescUnmapUnsafeNotImplemented,
  NaClDescUnmapNotImplemented,
  NaClDescReadNotImplemented,
  NaClDescWriteNotImplemented,
  NaClDescSeekNotImplemented,
  NaClDescIoctlNotImplemented,
  NaClDescFstatNotImplemented,
  NaClDescImcDescClose,
  NaClDescGetdentsNotImplemented,
  NACL_DESC_CONNECTED_SOCKET,
  NaClDescImcDescExternalizeSize,
  NaClDescImcDescExternalize,
  NaClDescLockNotImplemented,
  NaClDescTryLockNotImplemented,
  NaClDescUnlockNotImplemented,
  NaClDescWaitNotImplemented,
  NaClDescTimedWaitAbsNotImplemented,
  NaClDescSignalNotImplemented,
  NaClDescBroadcastNotImplemented,
  NaClDescImcDescSendMsg,
  NaClDescImcDescRecvMsg,
  NaClDescConnectAddrNotImplemented,
  NaClDescAcceptConnNotImplemented,
  NaClDescPostNotImplemented,
  NaClDescSemWaitNotImplemented,
  NaClDescGetValueNotImplemented,
};

int NaClDescImcDescInternalize(struct NaClDesc          **baseptr,
                               struct NaClDescXferState *xfer) {
  int                     rv;
  NaClHandle              h;
  struct NaClDescImcDesc  *ndidp;

  rv = -NACL_ABI_EIO;
  h = NACL_INVALID_HANDLE;
  ndidp = NULL;

  if (xfer->next_handle == xfer->handle_buffer_end) {
    rv = -NACL_ABI_EIO;
    goto cleanup;
  }
  ndidp = malloc(sizeof *ndidp);
  if (NULL == ndidp) {
    rv = -NACL_ABI_ENOMEM;
    goto cleanup;
  }
  NaClDescImcDescCtor(ndidp, *xfer->next_handle);
  *xfer->next_handle++ = NACL_INVALID_HANDLE;
  *baseptr = (struct NaClDesc *) ndidp;
  rv = 0;

cleanup:
  if (rv < 0) {
    free(ndidp);
  }
  return rv;
}

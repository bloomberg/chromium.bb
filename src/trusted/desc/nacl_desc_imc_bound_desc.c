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
 * Bound IMC descriptors.
 */

#include <stdlib.h>

#include <errno.h>

#include "native_client/src/include/portability.h"
#include "native_client/src/shared/imc/nacl_imc_c.h"

#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/desc/nacl_desc_effector.h"
#include "native_client/src/trusted/desc/nacl_desc_imc.h"
#include "native_client/src/trusted/desc/nacl_desc_imc_bound_desc.h"

#include "native_client/src/trusted/platform/nacl_log.h"

#include "native_client/src/trusted/service_runtime/nacl_config.h"
#include "native_client/src/trusted/service_runtime/include/sys/errno.h"


int NaClDescImcBoundDescCtor(struct NaClDescImcBoundDesc  *self,
                             NaClHandle                   h) {
  struct NaClDesc *basep = (struct NaClDesc *) self;

  basep->vtbl = (struct NaClDescVtbl *) NULL;
  if (!NaClDescCtor(basep)) {
    return 0;
  }
  self->h = h;
  basep->vtbl = &kNaClDescImcBoundDescVtbl;
  return 1;
}

void NaClDescImcBoundDescDtor(struct NaClDesc *vself) {
  struct NaClDescImcBoundDesc *self = (struct NaClDescImcBoundDesc *) vself;

  NaClClose(self->h);
  self->h = NACL_INVALID_HANDLE;
  vself->vtbl = (struct NaClDescVtbl *) NULL;
  NaClDescDtor(vself);
  return;
}

int NaClDescImcBoundDescClose(struct NaClDesc         *vself,
                              struct NaClDescEffector *effp) {
  NaClDescUnref(vself);
  return 0;
}

int NaClDescImcBoundDescExternalizeSize(struct NaClDesc *vself,
                                        size_t          *nbytes,
                                        size_t          *nhandles) {
  *nbytes = 0;
  *nhandles = 1;

  return 0;
}

int NaClDescImcBoundDescExternalize(struct NaClDesc           *vself,
                                    struct NaClDescXferState  *xfer) {
  struct NaClDescImcBoundDesc *self;

  self = (struct NaClDescImcBoundDesc *) vself;
  *xfer->next_handle++ = self->h;

  return 0;
}

int NaClDescImcBoundDescAcceptConn(struct NaClDesc          *vself,
                                   struct NaClDescEffector  *effp) {
  /*
   * See NaClDescConnCapConnectAddr code in nacl_desc_conn_cap.c
   */
  struct NaClDescImcBoundDesc *self;
  int                         retval;
  NaClHandle                  nh;
  int                         nbytes;
  struct NaClMessageHeader    conn_msg;
  struct NaClDescImcDesc      *peer;

  self = (struct NaClDescImcBoundDesc *) vself;

  if (NULL == (peer = malloc(sizeof *peer))) {
    return -NACL_ABI_ENOMEM;
  }

  conn_msg.iov = 0;
  conn_msg.iov_length = 0;
  conn_msg.handle_count = 1;
  conn_msg.handles = &nh;
  conn_msg.flags = 0;
  nh = NACL_INVALID_HANDLE;

  NaClLog(3,
          ("NaClDescImcBoundDescAcceptConn(0x%08"PRIxPTR", 0x%08"PRIxPTR"):"
           " h = %d\n"),
          (uintptr_t) vself,
          (uintptr_t) effp,
          self->h);

  if (-1 == (nbytes = NaClReceiveDatagram(self->h, &conn_msg, 0))) {
    NaClLog(LOG_ERROR,
            ("NaClDescImcBoundDescAcceptConn:"
             " could not receive connection message, errno %d\n"),
            errno);
    retval = -NACL_ABI_EMFILE;
    goto cleanup;
  }
  if (0 != nbytes) {
    NaClLog(LOG_ERROR, ("NaClDescImcBoundDescAcceptConn:"
                        " connection message contains data?!?\n"));
    retval = -NACL_ABI_EMFILE;  /* TODO(bsy): better errno? */
    goto cleanup;
  }
  if (1 != conn_msg.handle_count) {
    NaClLog(LOG_ERROR, ("NaClDescImcBoundDescAcceptConn: connection"
                        " message contains %"PRIdS" descriptors?!?\n"),
            conn_msg.handle_count);
    retval = -NACL_ABI_EMFILE;  /* TODO(bsy): better errno? */
    goto cleanup;
  }
  if (!NaClDescImcDescCtor(peer, nh)) {
    retval = -NACL_ABI_EMFILE;
    goto cleanup;
  }
  nh = NACL_INVALID_HANDLE;

  retval = (*effp->vtbl->ReturnCreatedDesc)(effp, (struct NaClDesc *) peer);

  if (retval < 0) {
    (*peer->base.vtbl->Dtor)((struct NaClDesc *) peer);
  }

cleanup:
  if (retval < 0) {
    if (NACL_INVALID_HANDLE != nh) {
      (void) NaClClose(nh);
      nh = NACL_INVALID_HANDLE;
    }
    free(peer);
  }
  return retval;
}

struct NaClDescVtbl const kNaClDescImcBoundDescVtbl = {
  NaClDescImcBoundDescDtor,
  NaClDescMapNotImplemented,
  NaClDescUnmapUnsafeNotImplemented,
  NaClDescUnmapNotImplemented,
  NaClDescReadNotImplemented,
  NaClDescWriteNotImplemented,
  NaClDescSeekNotImplemented,
  NaClDescIoctlNotImplemented,
  NaClDescFstatNotImplemented,
  NaClDescImcBoundDescClose,
  NaClDescGetdentsNotImplemented,
  NACL_DESC_BOUND_SOCKET,
  NaClDescImcBoundDescExternalizeSize,
  NaClDescImcBoundDescExternalize,
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
  NaClDescImcBoundDescAcceptConn,
  NaClDescPostNotImplemented,
  NaClDescSemWaitNotImplemented,
  NaClDescGetValueNotImplemented,
};

int NaClDescImcBoundDescInternalize(struct NaClDesc           **baseptr,
                                    struct NaClDescXferState  *xfer) {
  int                         rv;
  struct NaClDescImcBoundDesc *ndibdp;

  rv = -NACL_ABI_EIO;
  ndibdp = NULL;

  if (xfer->next_handle == xfer->handle_buffer_end) {
    rv = -NACL_ABI_EIO;
    goto cleanup;
  }
  ndibdp = malloc(sizeof *ndibdp);
  if (NULL == ndibdp) {
    goto cleanup;
  }
  NaClDescImcBoundDescCtor(ndibdp, *xfer->next_handle);
  *xfer->next_handle++ = NACL_INVALID_HANDLE;
  *baseptr = (struct NaClDesc *) ndibdp;
  rv = 0;

cleanup:
  if (rv < 0) {
    free(ndibdp);
  }
  return rv;
}

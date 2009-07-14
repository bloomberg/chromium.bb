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
 * Connection capabilities.
 */

#include <stdlib.h>
#include <string.h>

#include "native_client/src/include/portability.h"

#include "native_client/src/shared/imc/nacl_imc_c.h"
#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/desc/nacl_desc_imc.h"

#include "native_client/src/trusted/platform/nacl_log.h"
#include "native_client/src/trusted/service_runtime/nacl_config.h"
#include "native_client/src/trusted/service_runtime/include/sys/errno.h"


int NaClDescConnCapCtor(struct NaClDescConnCap          *self,
                        struct NaClSocketAddress const  *nsap) {
  struct NaClDesc *basep = (struct NaClDesc *) self;

  basep->vtbl = (struct NaClDescVtbl *) NULL;
  if (!NaClDescCtor(basep)) {
    return 0;
  }
  self->cap = *nsap;
  basep->vtbl = &kNaClDescConnCapVtbl;
  return 1;
}

void NaClDescConnCapDtor(struct NaClDesc *vself) {
  vself->vtbl = (struct NaClDescVtbl *) NULL;
  NaClDescDtor(vself);
  return;
}

int NaClDescConnCapClose(struct NaClDesc          *vself,
                         struct NaClDescEffector  *effp) {
  NaClDescUnref(vself);
  return 0;
}

int NaClDescConnCapExternalizeSize(struct NaClDesc  *vself,
                                   size_t           *nbytes,
                                   size_t           *nhandles) {
  *nbytes = NACL_PATH_MAX;
  *nhandles = 0;

  return 0;
}

int NaClDescConnCapExternalize(struct NaClDesc          *vself,
                               struct NaClDescXferState *xfer) {
  struct NaClDescConnCap    *self;

  self = (struct NaClDescConnCap *) vself;
  memcpy(xfer->next_byte, self->cap.path, NACL_PATH_MAX);
  xfer->next_byte += NACL_PATH_MAX;

  return 0;
}

int NaClDescConnCapConnectAddr(struct NaClDesc          *vself,
                               struct NaClDescEffector  *effp) {
  /*
   * See NaClDescImcBoundDescAcceptConn code in
   * nacl_desc_imc_bound_desc.c
   */
  struct NaClDescConnCap      *self;
  struct NaClDescImcBoundDesc *sender;
  int                         retval;
  NaClHandle                  nh[2];
  struct NaClMessageHeader    conn_msg;
  struct NaClDescImcDesc      *peer;

  NaClLog(3, "Entered NaClDescConnCapConnectAddr\n");
  self = (struct NaClDescConnCap *) vself;

  sender = (*effp->vtbl->SourceSock)(effp);
  if (NULL == sender) {
    NaClLog(LOG_ERROR, "NaClDescConnCapConnectAddr: service socket NULL\n");
    return -NACL_ABI_EIO;
  }

  NaClLog(4, " socket address %.*s\n", NACL_PATH_MAX, self->cap.path);

  if (NULL == (peer = malloc(sizeof *peer))) {
    return -NACL_ABI_ENOMEM;
  }

  if (0 != NaClSocketPair(nh)) {
    return -NACL_ABI_EMFILE;
  }

  conn_msg.iov_length = 0;
  conn_msg.handles = &nh[0];
  conn_msg.handle_count = 1;  /* send nh[0], keep nh[1] */
  conn_msg.flags = 0;

  NaClLog(4, " sending connection message\n");
  if (-1 == NaClSendDatagramTo(sender->h,
                               &conn_msg,
                               0,
                               &self->cap)) {
    NaClLog(LOG_ERROR, ("NaClDescConnCapConnectAddr:"
                        " initial connect message could not be sent.\n"));
    retval = -NACL_ABI_EMFILE;
    goto cleanup;
  }

  (void) NaClClose(nh[0]);
  nh[0] = NACL_INVALID_HANDLE;
  NaClLog(4, " creating NaClDescImcDesc for local end of socketpair\n");
  if (!NaClDescImcDescCtor(peer, nh[1])) {
    retval = -NACL_ABI_EMFILE;  /* TODO(bsy): is this the right errno? */
    goto cleanup;
  }
  nh[1] = NACL_INVALID_HANDLE;

  retval = (*effp->vtbl->ReturnCreatedDesc)(effp, ((struct NaClDesc *) peer));

cleanup:
  if (retval < 0) {
    NaClLog(4, " error return; cleaning up\n");
    if (NACL_INVALID_HANDLE != nh[0])
      (void) NaClClose(nh[0]);
    if (NACL_INVALID_HANDLE != nh[1])
      (void) NaClClose(nh[1]);
    /* peer is not constructed, so we need only to free the memory */
    free(peer);
  }
  return retval;
}

int NaClDescConnCapAcceptConn(struct NaClDesc         *vself,
                              struct NaClDescEffector *effp) {
  NaClLog(LOG_ERROR, "NaClDescConnCapAcceptConn: not IMC\n");
  return -NACL_ABI_EINVAL;
}

struct NaClDescVtbl const kNaClDescConnCapVtbl = {
  NaClDescConnCapDtor,
  NaClDescMapNotImplemented,
  NaClDescUnmapUnsafeNotImplemented,
  NaClDescUnmapNotImplemented,
  NaClDescReadNotImplemented,
  NaClDescWriteNotImplemented,
  NaClDescSeekNotImplemented,
  NaClDescIoctlNotImplemented,
  NaClDescFstatNotImplemented,
  NaClDescConnCapClose,
  NaClDescGetdentsNotImplemented,
  NACL_DESC_CONN_CAP,
  NaClDescConnCapExternalizeSize,
  NaClDescConnCapExternalize,
  NaClDescLockNotImplemented,
  NaClDescTryLockNotImplemented,
  NaClDescUnlockNotImplemented,
  NaClDescWaitNotImplemented,
  NaClDescTimedWaitAbsNotImplemented,
  NaClDescSignalNotImplemented,
  NaClDescBroadcastNotImplemented,
  NaClDescSendMsgNotImplemented,
  NaClDescRecvMsgNotImplemented,
  NaClDescConnCapConnectAddr,
  NaClDescConnCapAcceptConn,
  NaClDescPostNotImplemented,
  NaClDescSemWaitNotImplemented,
  NaClDescGetValueNotImplemented,
};

int NaClDescConnCapInternalize(struct NaClDesc          **baseptr,
                               struct NaClDescXferState *xfer) {
  int                       rv;
  struct NaClSocketAddress  nsa;
  struct NaClDescConnCap    *ndccp;

  rv = -NACL_ABI_EIO;  /* catch-all */
  ndccp = NULL;

  if (xfer->next_byte + NACL_PATH_MAX > xfer->byte_buffer_end) {
    rv = -NACL_ABI_EIO;
    goto cleanup;
  }
  memcpy(nsa.path, xfer->next_byte, NACL_PATH_MAX);
  ndccp = malloc(sizeof *ndccp);
  if (NULL == ndccp) {
    rv = -NACL_ABI_ENOMEM;
    goto cleanup;
  }
  if (!NaClDescConnCapCtor(ndccp, &nsa)) {
    rv = -NACL_ABI_ENOMEM;
    goto cleanup;
  }
  *baseptr = (struct NaClDesc *) ndccp;
  rv = 0;
  xfer->next_byte += NACL_PATH_MAX;
cleanup:
  if (rv < 0) {
    free(ndccp);
  }
  return rv;
}

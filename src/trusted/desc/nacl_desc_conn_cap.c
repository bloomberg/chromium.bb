/*
 * Copyright 2008  The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * NaCl Service Runtime.  I/O Descriptor / Handle abstraction.
 * Connection capabilities.
 */

#include <stdlib.h>
#include <string.h>

#include "native_client/src/include/portability.h"
#include "native_client/src/include/nacl_macros.h"

#include "native_client/src/shared/imc/nacl_imc_c.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/desc/nacl_desc_effector.h"
#include "native_client/src/trusted/desc/nacl_desc_imc.h"


#include "native_client/src/trusted/service_runtime/nacl_config.h"
#include "native_client/src/trusted/service_runtime/include/sys/errno.h"
#include "native_client/src/trusted/service_runtime/include/sys/stat.h"


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

int NaClDescConnCapFstat(struct NaClDesc          *vself,
                         struct NaClDescEffector  *effp,
                         struct nacl_abi_stat     *statbuf) {
  UNREFERENCED_PARAMETER(vself);
  UNREFERENCED_PARAMETER(effp);

  memset(statbuf, 0, sizeof *statbuf);
  statbuf->nacl_abi_st_mode = NACL_ABI_S_IFSOCKADDR | NACL_ABI_S_IRWXU;
  return 0;
}

int NaClDescConnCapClose(struct NaClDesc          *vself,
                         struct NaClDescEffector  *effp) {
  UNREFERENCED_PARAMETER(effp);

  NaClDescUnref(vself);
  return 0;
}

int NaClDescConnCapExternalizeSize(struct NaClDesc  *vself,
                                   size_t           *nbytes,
                                   size_t           *nhandles) {
  UNREFERENCED_PARAMETER(vself);

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
  size_t                      ix;
  struct NaClMessageHeader    conn_msg;
  struct NaClDescImcDesc      *peer;

  NaClLog(3, "Entered NaClDescConnCapConnectAddr\n");
  self = (struct NaClDescConnCap *) vself;

  peer = NULL;
  for (ix = 0; ix < NACL_ARRAY_SIZE(nh); ++ix) {
    nh[ix] = NACL_INVALID_HANDLE;
  }

  sender = (*effp->vtbl->SourceSock)(effp);
  if (NULL == sender) {
    NaClLog(LOG_ERROR, "NaClDescConnCapConnectAddr: service socket NULL\n");
    retval = -NACL_ABI_EIO;
    goto cleanup;
  }

  NaClLog(4, " socket address %.*s\n", NACL_PATH_MAX, self->cap.path);

  if (NULL == (peer = malloc(sizeof *peer))) {
    retval = -NACL_ABI_ENOMEM;
    goto cleanup;
  }

  if (0 != NaClSocketPair(nh)) {
    retval = -NACL_ABI_EMFILE;
    goto cleanup;
  }

  conn_msg.iov_length = 0;
  conn_msg.iov = NULL;
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
  if (retval < 0) {
    /* peer is fully constructed, so we cannot simply free it at this pt */
    NaClDescUnref((struct NaClDesc *) peer);
    peer = NULL;
  }

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
  UNREFERENCED_PARAMETER(vself);
  UNREFERENCED_PARAMETER(effp);

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
  NaClDescConnCapFstat,
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

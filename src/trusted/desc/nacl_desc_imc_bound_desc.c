/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * NaCl Service Runtime.  I/O Descriptor / Handle abstraction.
 * Bound IMC descriptors.
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "native_client/src/include/portability.h"
#include "native_client/src/shared/imc/nacl_imc_c.h"
#include "native_client/src/shared/platform/nacl_log.h"

#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/desc/nacl_desc_effector.h"
#include "native_client/src/trusted/desc/nacl_desc_imc.h"
#include "native_client/src/trusted/desc/nacl_desc_imc_bound_desc.h"

#include "native_client/src/trusted/service_runtime/nacl_config.h"
#include "native_client/src/trusted/service_runtime/include/sys/errno.h"
#include "native_client/src/trusted/service_runtime/include/sys/stat.h"


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

int NaClDescImcBoundDescFstat(struct NaClDesc          *vself,
                              struct NaClDescEffector  *effp,
                              struct nacl_abi_stat     *statbuf) {
  UNREFERENCED_PARAMETER(vself);
  UNREFERENCED_PARAMETER(effp);

  memset(statbuf, 0, sizeof *statbuf);
  statbuf->nacl_abi_st_mode = NACL_ABI_S_IFBOUNDSOCK;
  return 0;
}

int NaClDescImcBoundDescClose(struct NaClDesc         *vself,
                              struct NaClDescEffector *effp) {
  UNREFERENCED_PARAMETER(effp);

  NaClDescUnref(vself);
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
          ("NaClDescImcBoundDescAcceptConn(0x%08"NACL_PRIxPTR", "
           "0x%08"NACL_PRIxPTR"):"
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
                        " message contains %"NACL_PRIdS" descriptors?!?\n"),
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
    (*peer->base.base.vtbl->Dtor)((struct NaClDesc *) peer);
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
  NaClDescImcBoundDescFstat,
  NaClDescImcBoundDescClose,
  NaClDescGetdentsNotImplemented,
  NACL_DESC_BOUND_SOCKET,
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

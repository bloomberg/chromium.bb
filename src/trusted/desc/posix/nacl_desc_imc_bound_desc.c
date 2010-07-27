/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * NaCl Service Runtime.  I/O Descriptor / Handle abstraction.
 * Bound IMC descriptors.
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include "native_client/src/include/portability.h"
#include "native_client/src/shared/imc/nacl_imc_c.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_log.h"

#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/desc/nacl_desc_effector.h"
#include "native_client/src/trusted/desc/nacl_desc_imc.h"
#include "native_client/src/trusted/desc/nacl_desc_imc_bound_desc.h"

#include "native_client/src/trusted/service_runtime/nacl_config.h"
#include "native_client/src/trusted/service_runtime/include/sys/errno.h"
#include "native_client/src/trusted/service_runtime/include/sys/stat.h"


void NaClDescImcBoundDescDtor(struct NaClDesc *vself) {
  struct NaClDescImcBoundDesc *self = (struct NaClDescImcBoundDesc *) vself;

  NaClClose(self->h);
  self->h = NACL_INVALID_HANDLE;
  vself->vtbl = (struct NaClDescVtbl *) NULL;
  NaClDescDtor(vself);
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
  struct NaClDescImcBoundDesc *self = (struct NaClDescImcBoundDesc *) vself;
  int retval;
  NaClHandle received_fd;
  struct NaClDescImcDesc *peer;
  struct iovec iovec;
  struct msghdr accept_msg;
  struct cmsghdr *cmsg;
  char data_buf[1];
  char control_buf[CMSG_SPACE(sizeof(int))];
  int received;

  peer = malloc(sizeof(*peer));
  if (peer == NULL) {
    return -NACL_ABI_ENOMEM;
  }

  iovec.iov_base = data_buf;
  iovec.iov_len = sizeof(data_buf);
  accept_msg.msg_iov = &iovec;
  accept_msg.msg_iovlen = 1;
  accept_msg.msg_name = NULL;
  accept_msg.msg_namelen = 0;
  accept_msg.msg_control = control_buf;
  accept_msg.msg_controllen = sizeof(control_buf);

  received = recvmsg(self->h, &accept_msg, 0);
  if (received != 1 || data_buf[0] != 'c') {
    return -NACL_ABI_EIO;
  }

  received_fd = -1;
  for (cmsg = CMSG_FIRSTHDR(&accept_msg);
       cmsg != NULL;
       cmsg = CMSG_NXTHDR(&accept_msg, cmsg)) {
    if (cmsg->cmsg_level == SOL_SOCKET &&
        cmsg->cmsg_type == SCM_RIGHTS &&
        cmsg->cmsg_len == CMSG_LEN(sizeof(int))) {
      CHECK(received_fd == -1);
      /* We use memcpy() rather than assignment through a cast to avoid
         strict-aliasing warnings */
      memcpy(&received_fd, CMSG_DATA(cmsg), sizeof(int));
    }
  }
  if (received_fd == -1) {
    return -NACL_ABI_EIO;
  }

  if (!NaClDescImcDescCtor(peer, received_fd)) {
    retval = -NACL_ABI_EMFILE;
    goto cleanup;
  }
  received_fd = NACL_INVALID_HANDLE;

  retval = (*effp->vtbl->ReturnCreatedDesc)(effp, (struct NaClDesc *) peer);
  if (retval < 0) {
    (*peer->base.base.vtbl->Dtor)((struct NaClDesc *) peer);
  }

cleanup:
  if (retval < 0) {
    if (received_fd != NACL_INVALID_HANDLE) {
      NaClClose(received_fd);
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

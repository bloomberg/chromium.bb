/*
 * Copyright 2010  The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * NaCl Service Runtime.  I/O Descriptor / Handle abstraction.
 * Connection capabilities.
 */

#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability.h"

#include "native_client/src/shared/imc/nacl_imc_c.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/desc/nacl_desc_effector.h"
#include "native_client/src/trusted/desc/nacl_desc_imc.h"
#include "native_client/src/trusted/service_runtime/include/sys/errno.h"
#include "native_client/src/trusted/service_runtime/include/sys/stat.h"
#include "native_client/src/trusted/service_runtime/nacl_config.h"


void NaClDescConnCapFdDtor(struct NaClDesc *vself) {
  struct NaClDescConnCapFd *self = (struct NaClDescConnCapFd *) vself;

  NaClClose(self->connect_fd);
  self->connect_fd = NACL_INVALID_HANDLE;
  vself->vtbl = (struct NaClDescVtbl *) NULL;
  NaClDescDtor(vself);
  return;
}

int NaClDescConnCapFdFstat(struct NaClDesc          *vself,
                           struct NaClDescEffector  *effp,
                           struct nacl_abi_stat     *statbuf) {
  UNREFERENCED_PARAMETER(vself);
  UNREFERENCED_PARAMETER(effp);

  memset(statbuf, 0, sizeof *statbuf);
  statbuf->nacl_abi_st_mode = NACL_ABI_S_IFSOCKADDR | NACL_ABI_S_IRWXU;
  return 0;
}

int NaClDescConnCapFdClose(struct NaClDesc          *vself,
                           struct NaClDescEffector  *effp) {
  UNREFERENCED_PARAMETER(effp);

  NaClDescUnref(vself);
  return 0;
}

int NaClDescConnCapFdExternalizeSize(struct NaClDesc  *vself,
                                     size_t           *nbytes,
                                     size_t           *nhandles) {
  UNREFERENCED_PARAMETER(vself);

  *nbytes = 0;
  *nhandles = 1;

  return 0;
}

int NaClDescConnCapFdExternalize(struct NaClDesc          *vself,
                                 struct NaClDescXferState *xfer) {
  struct NaClDescConnCapFd    *self;

  self = (struct NaClDescConnCapFd *) vself;
  *xfer->next_handle++ = self->connect_fd;

  return 0;
}

int NaClDescConnCapFdConnectAddr(struct NaClDesc *vself,
                                 struct NaClDesc **result) {
  struct NaClDescConnCapFd *self = (struct NaClDescConnCapFd *) vself;
  NaClHandle sock_pair[2];
  struct NaClDescImcDesc *connected_socket;
  char control_buf[CMSG_SPACE(sizeof(int))];
  struct iovec iovec;
  struct msghdr connect_msg;
  struct cmsghdr *cmsg;
  int sent;

  if (NaClSocketPair(sock_pair) != 0) {
    return -NACL_ABI_EMFILE;
  }

  iovec.iov_base = "c";
  iovec.iov_len = 1;
  connect_msg.msg_iov = &iovec;
  connect_msg.msg_iovlen = 1;
  connect_msg.msg_name = NULL;
  connect_msg.msg_namelen = 0;
  connect_msg.msg_control = control_buf;
  connect_msg.msg_controllen = sizeof(control_buf);

  cmsg = CMSG_FIRSTHDR(&connect_msg);
  cmsg->cmsg_len = CMSG_LEN(sizeof(int));
  cmsg->cmsg_level = SOL_SOCKET;
  cmsg->cmsg_type = SCM_RIGHTS;
  /* We use memcpy() rather than assignment through a cast to avoid
     strict-aliasing warnings */
  memcpy(CMSG_DATA(cmsg), &sock_pair[0], sizeof(int));

  sent = sendmsg(self->connect_fd, &connect_msg, 0);
  NaClClose(sock_pair[0]);
  if (sent != 1) {
    NaClClose(sock_pair[1]);
    return -NACL_ABI_EIO;
  }

  connected_socket = malloc(sizeof(*connected_socket));
  if (connected_socket == NULL ||
      !NaClDescImcDescCtor(connected_socket, sock_pair[1])) {
    NaClClose(sock_pair[1]);
    free(connected_socket);
    return -NACL_ABI_ENOMEM;
  }

  *result = (struct NaClDesc *) connected_socket;
  return 0;
}

int NaClDescConnCapFdAcceptConn(struct NaClDesc *vself,
                                struct NaClDesc **result) {
  UNREFERENCED_PARAMETER(vself);
  UNREFERENCED_PARAMETER(result);

  NaClLog(LOG_ERROR, "NaClDescConnCapFdAcceptConn: not IMC\n");
  return -NACL_ABI_EINVAL;
}

struct NaClDescVtbl const kNaClDescConnCapFdVtbl = {
  NaClDescConnCapFdDtor,
  NaClDescMapNotImplemented,
  NaClDescUnmapUnsafeNotImplemented,
  NaClDescUnmapNotImplemented,
  NaClDescReadNotImplemented,
  NaClDescWriteNotImplemented,
  NaClDescSeekNotImplemented,
  NaClDescIoctlNotImplemented,
  NaClDescConnCapFdFstat,
  NaClDescConnCapFdClose,
  NaClDescGetdentsNotImplemented,
  NACL_DESC_CONN_CAP_FD,
  NaClDescConnCapFdExternalizeSize,
  NaClDescConnCapFdExternalize,
  NaClDescLockNotImplemented,
  NaClDescTryLockNotImplemented,
  NaClDescUnlockNotImplemented,
  NaClDescWaitNotImplemented,
  NaClDescTimedWaitAbsNotImplemented,
  NaClDescSignalNotImplemented,
  NaClDescBroadcastNotImplemented,
  NaClDescSendMsgNotImplemented,
  NaClDescRecvMsgNotImplemented,
  NaClDescConnCapFdConnectAddr,
  NaClDescConnCapFdAcceptConn,
  NaClDescPostNotImplemented,
  NaClDescSemWaitNotImplemented,
  NaClDescGetValueNotImplemented,
};

int NaClDescConnCapFdInternalize(struct NaClDesc          **result,
                                 struct NaClDescXferState *xfer) {
  struct NaClDescConnCapFd *conn_cap;

  if (xfer->next_handle == xfer->handle_buffer_end) {
    return -NACL_ABI_EIO;
  }
  conn_cap = malloc(sizeof(*conn_cap));
  if (conn_cap == NULL) {
    return -NACL_ABI_ENOMEM;
  }
  if (!NaClDescCtor(&conn_cap->base)) {
    free(conn_cap);
    return -NACL_ABI_ENOMEM;
  }
  conn_cap->base.vtbl = &kNaClDescConnCapFdVtbl;
  conn_cap->connect_fd = *xfer->next_handle;
  *xfer->next_handle++ = NACL_INVALID_HANDLE;
  *result = &conn_cap->base;
  return 0;
}

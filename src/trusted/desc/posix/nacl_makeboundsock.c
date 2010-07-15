/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/service_runtime/include/sys/errno.h"


int32_t NaClCommonDescMakeBoundSock(struct NaClDesc *pair[2]) {
  struct NaClDescConnCapFd *conn_cap = NULL;
  struct NaClDescImcBoundDesc *bound_sock = NULL;
  NaClHandle fd_pair[2];

  if (NaClSocketPair(fd_pair) != 0) {
    return -NACL_ABI_EMFILE;
  }

  conn_cap = malloc(sizeof(*conn_cap));
  if (conn_cap == NULL) {
    NaClLog(LOG_FATAL, "NaClCommonDescMakeBoundSock: allocation failed");
  }
  if (!NaClDescCtor(&conn_cap->base)) {
    NaClLog(LOG_FATAL, "NaClCommonDescMakeBoundSock: NaClDescCtor failed");
  }
  conn_cap->base.vtbl = &kNaClDescConnCapFdVtbl;
  conn_cap->connect_fd = fd_pair[0];

  bound_sock = malloc(sizeof(*bound_sock));
  if (bound_sock == NULL) {
    NaClLog(LOG_FATAL, "NaClCommonDescMakeBoundSock: allocation failed");
  }
  if (!NaClDescCtor(&bound_sock->base)) {
    NaClLog(LOG_FATAL, "NaClCommonDescMakeBoundSock: NaClDescCtor failed");
  }
  bound_sock->base.vtbl = &kNaClDescImcBoundDescVtbl;
  bound_sock->h = fd_pair[1];

  pair[0] = &bound_sock->base;
  pair[1] = &conn_cap->base;
  return 0;
}

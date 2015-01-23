/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <unistd.h>

#include "native_client/src/public/imc_syscalls.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/shared/srpc/nacl_srpc_internal.h"

#define BOUND_SOCKET 3


int NaClSrpcAcceptClientConnection(const struct NaClSrpcHandlerDesc *methods) {
  int sock_fd = -1;
  int result = 1;

  NaClSrpcLog(1, "NaClSrpcAcceptClientConnection(methods=%p)\n",
              (void*) methods);
  sock_fd = imc_accept(BOUND_SOCKET);
  if (sock_fd == -1) {
    NaClSrpcLog(NACL_SRPC_LOG_ERROR,
                "NaClSrpcAcceptClientConnection: imc_accept failed.\n");
    return 0;
  }
  if (!NaClSrpcServerLoop(sock_fd, methods, NULL)) {
    NaClSrpcLog(1,
                "NaClSrpcAcceptClientConnection: NaClSrpcServerLoop exited.\n");
    result = 0;
  }
  if (close(sock_fd) != 0) {
    NaClSrpcLog(NACL_SRPC_LOG_ERROR,
                "NaClSrpcAcceptClientConnection: close failed.\n");
    result = 0;
  }
  return result;
}

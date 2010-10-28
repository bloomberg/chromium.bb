/*
 * Copyright (c) 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <sys/nacl_syscalls.h>
#include <unistd.h>

#include "native_client/src/shared/srpc/nacl_srpc.h"

#define BOUND_SOCKET 3


int NaClSrpcAcceptClientConnection(const struct NaClSrpcHandlerDesc *methods) {
  int sock_fd = -1;
  int result = 1;

  if (NaClSrpcIsStandalone()) {
    return NaClSrpcCommandLoopMain(methods);
  }

  sock_fd = imc_accept(BOUND_SOCKET);
  if (sock_fd == -1) {
    return 0;
  }
  if (!NaClSrpcServerLoop(sock_fd, methods, NULL)) {
    result = 0;
  }
  if (close(sock_fd) != 0) {
    result = 0;
  }
  return result;
}

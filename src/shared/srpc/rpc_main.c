/*
 * Copyright (c) 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <sys/nacl_syscalls.h>
#include <unistd.h>

#include "native_client/src/shared/srpc/nacl_srpc.h"

#define BOUND_SOCKET 3


int NaClSrpcMain(const struct NaClSrpcHandlerDesc *methods) {
  if (NaClSrpcIsStandalone()) {
    return NaClSrpcCommandLoopMain(methods);
  } else {
    int sock_fd;
    int result = 0;

    sock_fd = imc_accept(BOUND_SOCKET);
    if (sock_fd == -1) {
      return 1;
    }
    if (!NaClSrpcServerLoop(sock_fd, methods, NULL)) {
      result = 1;
    }
    if (close(sock_fd) != 0) {
      result = 1;
    }
    return result;
  }
}

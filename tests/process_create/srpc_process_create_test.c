/*
 * Copyright (c) 2013 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <sys/fcntl.h>
#include <string.h>
#include <unistd.h>

#include "native_client/src/public/imc_syscalls.h"
#include "native_client/src/public/imc_types.h"
#include "native_client/src/public/name_service.h"

#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/shared/ldr/nacl_ldr.h"

#include "native_client/src/trusted/service_runtime/include/bits/nacl_syscalls.h"
#include "native_client/src/trusted/service_runtime/include/sys/nacl_kernel_service.h"

static int g_sock_addr;
static int g_app_addr;
static struct NaClSrpcChannel g_command_channel;

int CreateProcess(struct NaClSrpcChannel *kern_chan) {
  int status;

  if (NACL_SRPC_RESULT_OK !=
      NaClSrpcInvokeBySignature(kern_chan, NACL_KERNEL_SERVICE_CREATE_PROCESS,
                                &status, &g_sock_addr, &g_app_addr)) {
    fprintf(stderr, "create process failed, status %d\n", status);
    return 0;
  }
  assert(0 == status);

  if (!NaClLdrSetupCommandChannel(g_sock_addr, &g_command_channel)) {
    fprintf(stderr, "failed to setup command channel\n");
    return 0;
  }

  return 1;
}

int StartApplication(int nexe_fd) {
  int app_conn;
  struct NaClSrpcChannel app_channel;

  if (!NaClLdrLoadModule(&g_command_channel, nexe_fd)) {
    fprintf(stderr, "failed to load nexe\n");
    return 0;
  }

  if (!NaClLdrStartModule(&g_command_channel)) {
    fprintf(stderr, "failed to start module\n");
    return 0;
  }

  /*
   * We use the connection to the untrusted code just to wait until the
   * application finishes executing just to ensure that we don't close
   * the command channel prematurely killing the application.
   */
  app_conn = imc_connect(g_app_addr);
  assert(-1 != app_conn);

  if (!NaClSrpcClientCtor(&app_channel, app_conn)) {
    fprintf(stderr, "app srpc client channel ctor failed\n");
    return 1;
  }
  if (NACL_SRPC_RESULT_OK !=
      NaClSrpcInvokeBySignature(&app_channel, "hello::")) {
    fprintf(stderr, "hello rpc to subprogram failed\n");
    return 1;
  }

  close(g_app_addr);

  NaClSrpcDtor(&g_command_channel);

  return 1;
}

int main(void) {
  int ns;
  struct NaClSrpcChannel ns_channel;
  int connected_socket;
  int kernel;
  struct NaClSrpcChannel kernel_channel;
  int kernel_conn;
  int manifest;
  struct NaClSrpcChannel manifest_channel;
  int manifest_conn;
  int status;
  int nexe_fd;

  if (!NaClSrpcModuleInit()) {
    fprintf(stderr, "srpc module init failed\n");
    return 1;
  }
  ns = -1;
  nacl_nameservice(&ns);
  assert(-1 != ns);

  connected_socket = imc_connect(ns);
  assert(-1 != connected_socket);
  if (!NaClSrpcClientCtor(&ns_channel, connected_socket)) {
    fprintf(stderr, "ns srpc client channel ctor failed\n");
    return 1;
  }
  /* obtain manifest kernel service socket address */
  if (NACL_SRPC_RESULT_OK !=
      NaClSrpcInvokeBySignature(&ns_channel, NACL_NAME_SERVICE_LOOKUP,
                                "KernelService", O_RDWR, &status, &kernel)) {
    fprintf(stderr, "nameservice lookup failed, status %d\n", status);
    return 1;
  }
  assert(NACL_NAME_SERVICE_SUCCESS == status);
  /* connect to kernel service */
  kernel_conn = imc_connect(kernel);
  assert(-1 != kernel_conn);
  close(kernel);
  if (!NaClSrpcClientCtor(&kernel_channel, kernel_conn)) {
    fprintf(stderr, "kernel srpc client channel ctor failed\n");
    return 1;
  }
  /* obtain manifest name service socket address */
  if (NACL_SRPC_RESULT_OK !=
      NaClSrpcInvokeBySignature(&ns_channel, NACL_NAME_SERVICE_LOOKUP,
                                "ManifestNameService", O_RDWR,
                                &status, &manifest)) {
    fprintf(stderr, "nameservice lookup failed, status %d\n", status);
    return 1;
  }
  assert(NACL_NAME_SERVICE_SUCCESS == status);
  /* connect to manifest name service */
  manifest_conn = imc_connect(manifest);
  assert(-1 != manifest_conn);
  close(manifest);
  if (!NaClSrpcClientCtor(&manifest_channel, manifest_conn)) {
    fprintf(stderr, "manifest srpc client channel ctor failed\n");
    return 1;
  }
  if (NACL_SRPC_RESULT_OK !=
      NaClSrpcInvokeBySignature(&manifest_channel, NACL_NAME_SERVICE_LOOKUP,
                                "my_nexe", O_RDONLY, &status, &nexe_fd)) {
    fprintf(stderr, "nexe manifest service lookup failed\n");
    return 1;
  }
  assert(0 == status);
  /* create new child process */
  if (!CreateProcess(&kernel_channel)) {
    fprintf(stderr, "could not create process\n");
    return 1;
  }
  /* run the process */
  if (!StartApplication(nexe_fd)) {
    fprintf(stderr, "failed to start application\n");
    return 1;
  }

  return 0;
}

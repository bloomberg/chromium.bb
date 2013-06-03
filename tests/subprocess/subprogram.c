/*
 * Copyright (c) 2013 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "native_client/src/public/imc_syscalls.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"

void HelloHandler(struct NaClSrpcRpc *rpc,
                  struct NaClSrpcArg *args[],
                  struct NaClSrpcArg *rets[],
                  struct NaClSrpcClosure *done) {
  printf("Hello world\n");
  fflush(stdout);
  (*done->Run)(done);
}

struct NaClSrpcHandlerDesc const table[] = {
  { "hello::", HelloHandler, },
  { (char const *) NULL, (NaClSrpcMethod) NULL, },
};

void *RpcHandler(void *thread_state) {
  int desc = (int) thread_state;
  if (!NaClSrpcServerLoop(desc, table, NULL)) {
    fprintf(stderr, "NaClSrpcServerLoop failed\n");
    abort();
  }
  (void) close(desc);
  return (void *) NULL;
}

int main(void) {
  int d;

  NaClSrpcModuleInit();
  while ((d = imc_accept(3)) != -1) {
    pthread_t thr;
    if (0 != pthread_create(&thr, (pthread_attr_t *) NULL,
                            RpcHandler, (void *) d)) {
      perror("pthread_create for RpcHandler");
      abort();
    }
  }
  NaClSrpcModuleFini();
  return 0;
}

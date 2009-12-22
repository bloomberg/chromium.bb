/*
 * Copyright (c) 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * NaCl simple rpc service main loop
 */

#include <stdlib.h>

#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/shared/srpc/nacl_srpc_internal.h"

#include <sys/nacl_syscalls.h>

static NaClSrpcError Interpreter(NaClSrpcService* service,
                                 NaClSrpcChannel* channel,
                                 uint32_t rpc_number,
                                 NaClSrpcArg** ins,
                                 NaClSrpcArg** outs) {
  return (*NaClSrpcServiceMethod(service, rpc_number))(NULL, ins, outs);
}

int __attribute__ ((weak)) main(int argc, char* argv[]) {
  const int is_embedded = (-1 != srpc_get_fd());

  if (!is_embedded) {
    /* Build the service */
    NaClSrpcService* service = (NaClSrpcService*) malloc(sizeof(*service));
    if (NULL == service) {
      return 1;
    }
    if (!NaClSrpcServiceHandlerCtor(service, __kNaClSrpcHandlers)) {
      free(service);
      return 1;
    }
    /* TODO(sehr): Add the descriptor for the default socket address */
    /* Message processing loop. */
    NaClSrpcCommandLoop(service, NULL, Interpreter, kNaClSrpcInvalidImcDesc);
  }

  return 0;
}

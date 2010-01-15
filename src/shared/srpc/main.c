/*
 * Copyright (c) 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * NaCl simple rpc service main loop which starts an interpreter loop
 * for manual rpc invocation.
 * Note: this is a weak symbol and users are free to provide their
 *       own main().
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
  const int stand_alone = (-1 == srpc_get_fd());
  /* NOTE: stand_alone mode happens when a nacl_module is run directly
   * via sel_ldr not using sel_universal or a plugin
   */
  if (stand_alone) {
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
  /* NOTE: in the "else case" we implicitly call the sequence
   *  __srpc_init();
   *  __srpc_wait();
   *  via the startup code
   * TODO: make this code more straight forward
   */

  return 0;
}

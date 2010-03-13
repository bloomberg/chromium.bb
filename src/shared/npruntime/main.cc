/*
 * Copyright (c) 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * NaCl NPAPI rpc service main loop which starts an interpreter loop
 * for manual rpc invocation.
 * Note: This is a copy of that found in srpc/main.c, but without using
 *       a weak symbol.  This is justified because NPAPI plugins do not
 *       define their own main.
 */

#include <stdlib.h>
#include <sys/nacl_syscalls.h>
#include <new>

#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "gen/native_client/src/shared/npruntime/npnavigator_rpc.h"

static NaClSrpcError Interpreter(NaClSrpcService* service,
                                 NaClSrpcChannel* channel,
                                 uint32_t rpc_number,
                                 NaClSrpcArg** ins,
                                 NaClSrpcArg** outs) {
  return (*NaClSrpcServiceMethod(service, rpc_number))(NULL, ins, outs);
}

// The main definition here is almost identical to that found in
// src/untrusted/srpc/main.cc.  The definition is required to ensure that
// the plugin RPC definitions are resolved from the library.

int main(int argc, char* argv[]) {
  const int stand_alone = (-1 == srpc_get_fd());
  // NOTE: stand_alone mode happens when a nacl_module is run directly
  // via sel_ldr not using sel_universal or a plugin
  if (stand_alone) {
    // Build the service.
    NaClSrpcService* service = reinterpret_cast<NaClSrpcService*>(
        calloc(1, sizeof(*service)));
    if (NULL == service) {
      return 1;
    }
    if (!NaClSrpcServiceHandlerCtor(service, NPNavigatorRpcs::srpc_methods)) {
      free(service);
      return 1;
    }
    // The message processing loop used if sel_ldr is used by itself.
    NaClSrpcCommandLoop(service, NULL, Interpreter, kNaClSrpcInvalidImcDesc);
  }
  // If sel_ldr is invoked from sel_universal or the plugin, the message
  // processing loop is invoked from start.

  return 0;
}

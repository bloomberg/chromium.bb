/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Simple test for native web workers.
 */

#include <stdio.h>
#include <string.h>
#include <nacl/nacl_srpc.h>


int worker_upcall_desc = -1;

/*
 *  Send a string to a native web worker
 */
NaClSrpcError SetUpcallDesc(NaClSrpcChannel *channel,
                            NaClSrpcArg **in_args,
                            NaClSrpcArg **out_args) {
  worker_upcall_desc = in_args[0]->u.hval;
  return NACL_SRPC_RESULT_OK;
}

/*
 *  Send a string to a native web worker
 */
static char* message_string;

NaClSrpcError PostMessage(NaClSrpcChannel *channel,
                          NaClSrpcArg **in_args,
                          NaClSrpcArg **out_args) {
  /*
   * Strdup must be used because the SRPC layer frees the string passed to it.
   */
  message_string = strdup(in_args[0]->u.sval);
  /*
   * Echo the message back to the renderer.
   */
  if (-1 != worker_upcall_desc) {
    NaClSrpcChannel upcall_channel;
    if (!NaClSrpcClientCtor(&upcall_channel, worker_upcall_desc)) {
      return NACL_SRPC_RESULT_APP_ERROR;
    }
    NaClSrpcInvokeBySignature(&upcall_channel, "postMessage:s:", "from nacl");
    NaClSrpcDtor(&upcall_channel);
  }
  return NACL_SRPC_RESULT_OK;
}

const struct NaClSrpcHandlerDesc srpc_methods[] = {
  { "setUpcallDesc:h:", SetUpcallDesc },
  { "postMessage:s:", PostMessage },
  { NULL, NULL },
};

int main() {
  if (!NaClSrpcModuleInit()) {
    return 1;
  }
  if (!NaClSrpcAcceptClientConnection(srpc_methods)) {
    return 1;
  }
  NaClSrpcModuleFini();
  return 0;
}

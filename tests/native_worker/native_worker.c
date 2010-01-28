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
 * Export the method as taking a handle.
 */
NACL_SRPC_METHOD("setUpcallDesc:h:", SetUpcallDesc);

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
    NaClSrpcInvokeByName(&upcall_channel, "postMessage", "from nacl");
    NaClSrpcDtor(&upcall_channel);
  }
  return NACL_SRPC_RESULT_OK;
}

/*
 * Export the method as taking a string.
 */
NACL_SRPC_METHOD("postMessage:s:", PostMessage);

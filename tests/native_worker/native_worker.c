/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
int SetUpcallDesc(NaClSrpcChannel *channel,
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

int PostMessage(NaClSrpcChannel *channel,
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

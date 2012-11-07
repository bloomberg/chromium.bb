/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Another Fibonacci RPC method.
 */

#include <string.h>
#include <stdio.h>

#include "native_client/src/shared/srpc/nacl_srpc.h"

/*
 * FibonacciScalar is an rpc method that computes the vitally important
 * fibonacci recurrence.  The three input arguments are the first two
 * values of the sequence, and the index of the sequence element to compute.
 * That element of the sequence is returned in the output scalar.
 */
void FibonacciScalar(NaClSrpcRpc *rpc,
                     NaClSrpcArg **in_args,
                     NaClSrpcArg **out_args,
                     NaClSrpcClosure *done) {
  int v0 = in_args[0]->u.ival;
  int v1 = in_args[1]->u.ival;
  int v2;
  int num = in_args[2]->u.ival;

  if (num < 0) {
    rpc->result = NACL_SRPC_RESULT_APP_ERROR;
    done->Run(done);
    return;
  }
  while (num > 0) {
    v2 = v0 + v1;
    v0 = v1; v1 = v2;
    --num;
  }
  out_args[0]->u.ival = v0;
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

const struct NaClSrpcHandlerDesc srpc_methods[] = {
  { "fib:iii:i", FibonacciScalar },
  { NULL, NULL },
};

int main(void) {
  if (!NaClSrpcModuleInit()) {
    return 1;
  }
  if (!NaClSrpcAcceptClientConnection(srpc_methods)) {
    return 1;
  }
  NaClSrpcModuleFini();
  return 0;
}

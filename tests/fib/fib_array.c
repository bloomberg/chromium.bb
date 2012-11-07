/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * A Fibonacci RPC method.
 */


#include <stdlib.h>
#include <stdio.h>

#include "native_client/src/shared/srpc/nacl_srpc.h"

/*
 * FibonacciArray is an rpc method that computes the vitally important
 * fibonacci recurrence.  The two input arguments are the first two
 * values of the sequence.  The size of the output array determines how many
 * elements of the sequence to compute, which are returned in the output array.
 */
void FibonacciArray(NaClSrpcRpc *rpc,
                    NaClSrpcArg **in_args,
                    NaClSrpcArg **out_args,
                    NaClSrpcClosure *done) {
  int v0 = in_args[0]->u.ival;
  int v1 = in_args[1]->u.ival;
  int v2;
  int num = out_args[0]->u.count;
  int32_t *dest = out_args[0]->arrays.iarr;
  int i;

  if (num < 2) {
    rpc->result = NACL_SRPC_RESULT_APP_ERROR;
    done->Run(done);
    return;
  }
  *dest++ = v0;
  *dest++ = v1;
  for (i = 2; i < num; ++i) {
    v2 = v0 + v1;
    *dest++ = v2;
    v0 = v1; v1 = v2;
  }
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

const struct NaClSrpcHandlerDesc srpc_methods[] = {
  { "fib:ii:I", FibonacciArray },
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

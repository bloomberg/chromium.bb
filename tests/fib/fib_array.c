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
 * A Fibonacci RPC method.
 */


#include <stdlib.h>
#include <stdio.h>

#include <nacl/nacl_srpc.h>

/*
 * FibonacciArray is an rpc method that computes the vitally important
 * fibonacci recurrence.  The two input arguments are the first two
 * values of the sequence.  The size of the output array determines how many
 * elements of the sequence to compute, which are returned in the output array.
 */
NaClSrpcError FibonacciArray(NaClSrpcChannel *channel,
                             NaClSrpcArg **in_args,
                             NaClSrpcArg **out_args) {
  int v0 = in_args[0]->u.ival;
  int v1 = in_args[1]->u.ival;
  int v2;
  int num = out_args[0]->u.iaval.count;
  int *dest = out_args[0]->u.iaval.iarr;
  int i;

  if (num < 2) return NACL_SRPC_RESULT_APP_ERROR;
  *dest++ = v0;
  *dest++ = v1;
  for (i = 2; i < num; ++i) {
    v2 = v0 + v1;
    *dest++ = v2;
    v0 = v1; v1 = v2;
  }
  return NACL_SRPC_RESULT_OK;
}

NACL_SRPC_METHOD("fib:ii:I", FibonacciArray);

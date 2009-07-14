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
 * NaCl simple rpc dispatcher
 */

#include "nacl_srpc.h"
#include "nacl_srpc_internal.h"

int __NaClSrpcMain(unsigned int         rpc_num,
                   NaClSrpcHandlerDesc* rpc_desc,
                   unsigned int         num_rpcs,
                   NaClSrpcChannel*     channel,
                   struct NaClSrpcArg   **in_args,
                   struct NaClSrpcArg   **out_args)
{
  const char* entry_format;

  if (rpc_num >= num_rpcs) {
    return NACL_SRPC_RESULT_BAD_RPC_NUMBER;
  }

  entry_format = rpc_desc[rpc_num].entry_fmt;
  if (!NaClSrpcTypeCheckArgs(entry_format, in_args, out_args)) {
    /*
     * the service runtime should have perform type checks already,
     * and this code should be redundant.
     */
    return NACL_SRPC_RESULT_INTERNAL;
  }

  /* TODO(sehr) convert this to use the proper API. */
  return (*rpc_desc[rpc_num].handler)(channel, in_args, out_args);
}

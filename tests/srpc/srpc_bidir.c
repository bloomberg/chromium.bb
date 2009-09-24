/*
 * Copyright 2009, Google Inc.
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


#include <errno.h>
#include <nacl/nacl_srpc.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * SetUpcallServices delivers the service discovery string that describes
 * those client services that may be called back from the server.
 */
NaClSrpcError SetUpcallServices(NaClSrpcChannel *channel,
                                NaClSrpcArg **in_args,
                                NaClSrpcArg **out_args) {
  const char* sd_string = (const char*) in_args[0]->u.sval;
  NaClSrpcService* service;
  printf("SetUpcallServices: %s\n", sd_string);
  service = (NaClSrpcService*) malloc(sizeof(*service));
  if (NULL == service) {
    printf("malloc failed\n");
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  if (!NaClSrpcServiceStringCtor(service, sd_string)) {
    printf("CTOR failed\n");
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  channel->client = service;
  return NACL_SRPC_RESULT_OK;
}

NACL_SRPC_METHOD("set_upcall_services:s:", SetUpcallServices);


/*
 * TestUpcall requests a server test of a named client method.
 */
NaClSrpcError TestUpcall(NaClSrpcChannel *channel,
                         NaClSrpcArg **in_args,
                         NaClSrpcArg **out_args) {
  const char* method_name = (const char*) in_args[0]->u.sval;
  NaClSrpcError retval;

  printf("Testing upcall to method: '%s'\n", method_name);
  retval = NaClSrpcInvokeByName(channel, method_name, "hello");
  out_args[0]->u.ival = retval;

  return NACL_SRPC_RESULT_OK;
}

NACL_SRPC_METHOD("test_upcall:s:i", TestUpcall);

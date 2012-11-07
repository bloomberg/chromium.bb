/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "native_client/src/shared/srpc/nacl_srpc.h"


/*
 * SetUpcallServices delivers the service discovery string that describes
 * those client services that may be called back from the server.
 */
void SetUpcallServices(NaClSrpcRpc *rpc,
                       NaClSrpcArg **in_args,
                       NaClSrpcArg **out_args,
                       NaClSrpcClosure *done) {
  const char* sd_string = (const char*) in_args[0]->arrays.str;
  NaClSrpcService* service;
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  printf("SetUpcallServices: %s\n", sd_string);
  service = (NaClSrpcService*) malloc(sizeof(*service));
  if (NULL == service) {
    printf("malloc failed\n");
    done->Run(done);
    return;
  }
  if (!NaClSrpcServiceStringCtor(service, sd_string)) {
    printf("CTOR failed\n");
    done->Run(done);
    return;
  }
  rpc->channel->client = service;
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}


/*
 * TestUpcall requests a server test of a named client method.
 */
void TestUpcall(NaClSrpcRpc *rpc,
                NaClSrpcArg **in_args,
                NaClSrpcArg **out_args,
                NaClSrpcClosure *done) {
  const char* method_name = (const char*) in_args[0]->arrays.str;
  NaClSrpcError retval;

  printf("Testing upcall to method: '%s'\n", method_name);
  retval = NaClSrpcInvokeBySignature(rpc->channel, method_name, "hello::");
  out_args[0]->u.ival = retval;

  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

const struct NaClSrpcHandlerDesc srpc_methods[] = {
  { "set_upcall_services:s:", SetUpcallServices },
  { "test_upcall:s:i", TestUpcall },
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

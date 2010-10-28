/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
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


/*
 * TestUpcall requests a server test of a named client method.
 */
NaClSrpcError TestUpcall(NaClSrpcChannel *channel,
                         NaClSrpcArg **in_args,
                         NaClSrpcArg **out_args) {
  const char* method_name = (const char*) in_args[0]->u.sval;
  NaClSrpcError retval;

  printf("Testing upcall to method: '%s'\n", method_name);
  retval = NaClSrpcInvokeBySignature(channel, method_name, "hello::");
  out_args[0]->u.ival = retval;

  return NACL_SRPC_RESULT_OK;
}

const struct NaClSrpcHandlerDesc srpc_methods[] = {
  { "set_upcall_services:s:", SetUpcallServices },
  { "test_upcall:s:i", TestUpcall },
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

/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Simple test for simple rpc.
 */

#include <stdio.h>
#include <string.h>
#include <nacl/nacl_srpc.h>

/*
 *  Return a simple integer.
 */
NaClSrpcError FortyTwo(NaClSrpcChannel *channel,
                       NaClSrpcArg **in_args,
                       NaClSrpcArg **out_args) {
  out_args[0]->u.ival = 42;
  return NACL_SRPC_RESULT_OK;
}

/*
 *  Return a clever string.
 */
NaClSrpcError HelloWorld(NaClSrpcChannel *channel,
                         NaClSrpcArg **in_args,
                         NaClSrpcArg **out_args) {
  /*
   * Strdup must be used because the SRPC layer frees the string passed to it.
   */
  out_args[0]->u.sval = strdup("hello, world.");
  return NACL_SRPC_RESULT_OK;
}

const struct NaClSrpcHandlerDesc srpc_methods[] = {
  /* Export the method as taking no arguments and returning one integer. */
  { "fortytwo::i", FortyTwo },
  /* Export the method as taking no arguments and returning one integer. */
  { "helloworld::s", HelloWorld },
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

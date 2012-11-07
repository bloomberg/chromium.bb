/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Simple test for simple rpc.
 */

#include <stdio.h>
#include <string.h>

#include "native_client/src/shared/srpc/nacl_srpc.h"


/*
 *  Return a simple integer.
 */
void FortyTwo(NaClSrpcRpc *rpc,
              NaClSrpcArg **in_args,
              NaClSrpcArg **out_args,
              NaClSrpcClosure *done) {
  out_args[0]->u.ival = 42;
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

/*
 *  Return a clever string.
 */
void HelloWorld(NaClSrpcRpc *rpc,
                NaClSrpcArg **in_args,
                NaClSrpcArg **out_args,
                NaClSrpcClosure *done) {
  /*
   * Strdup must be used because the SRPC layer frees the string passed to it.
   */
  out_args[0]->arrays.str = strdup("hello, world.");
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

const struct NaClSrpcHandlerDesc srpc_methods[] = {
  /* Export the method as taking no arguments and returning one integer. */
  { "fortytwo::i", FortyTwo },
  /* Export the method as taking no arguments and returning one string. */
  { "helloworld::s", HelloWorld },
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

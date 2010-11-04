/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <nacl/nacl_srpc.h>
#include <sys/nacl_syscalls.h>


void TestMethod(NaClSrpcRpc *rpc,
                NaClSrpcArg **in_args,
                NaClSrpcArg **out_args,
                NaClSrpcClosure *done) {
  out_args[0]->u.sval = strdup("Deferred SRPC connection worked.");
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

const struct NaClSrpcHandlerDesc srpc_methods[] = {
  { "test_method::s", TestMethod },
  { NULL, NULL },
};

int main() {
  int fd;
  int rc;

  /* We ignore the first connection, which is initiated automatically
     by the plugin. */
  fd = imc_accept(3);
  assert(fd >= 0);
  rc = close(fd);
  assert(rc == 0);

  /* We also ignore the second connection, initiated by
     TestDeferredSrpcInit() using __startSrpcServices(), in order to
     test getting an error from __startSrpcServices(). */
  fd = imc_accept(3);
  assert(fd >= 0);
  rc = close(fd);
  assert(rc == 0);

  /* But the third connection was just right!  We accept it. */
  fd = imc_accept(3);
  assert(fd >= 0);
  NaClSrpcServerLoop(fd, srpc_methods, NULL);

  return 0;
}

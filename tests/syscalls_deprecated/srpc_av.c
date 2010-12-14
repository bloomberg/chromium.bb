/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Test deprecated AV syscalls and verify that they fail in Chrome.
 * These tests intentionally invoke at the syscall level.
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <nacl/nacl_srpc.h>

/*
 *  Return a string.
 *   "SUCCESS" - all tests passed
 *  !"SUCCESS" - string contains name and return value of failed test.
 *
 * The deprecated syscalls referenced in this test have been removed.
 * TODO(ncbray) remove this file
 */
void AVTest(NaClSrpcRpc *rpc,
            NaClSrpcArg **in_args,
            NaClSrpcArg **out_args,
            NaClSrpcClosure *done) {

  out_args[0]->arrays.str = strdup("SUCCESS");
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

const struct NaClSrpcHandlerDesc srpc_methods[] = {
  { "avtest::s", AVTest },
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

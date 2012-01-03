/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <string.h>

#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/tests/inbrowser_test_runner/test_runner.h"


/* We use SRPC to send the test results back to Javascript.  This was
   quick to write, but it has the significant disadvantage that it
   blocks the browser/renderer while the test is running.  This would
   not be good if we have tests that fail by hanging.
   TODO(mseaborn): We could switch to using async messaging instead. */

/* Use of a global variable is just a short cut, since
   NaClSrpcAcceptClientOnThread() does not take arguments besides
   function pointers. */
static int (*g_test_func)(void);

void RunTestsWrapper(NaClSrpcRpc *rpc,
                     NaClSrpcArg **in_args,
                     NaClSrpcArg **out_args,
                     NaClSrpcClosure *done) {
  int result = g_test_func();
  out_args[0]->arrays.str = strdup(result == 0 ? "passed" : "failed");
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

const struct NaClSrpcHandlerDesc srpc_methods[] = {
  { "run_tests::s", RunTestsWrapper },
  { NULL, NULL },
};

int RunTests(int (*test_func)(void)) {
  /* Turn off stdout buffering to aid debugging in case of a crash. */
  setvbuf(stdout, NULL, _IONBF, 0);
  if (NaClSrpcIsStandalone()) {
    return test_func();
  } else {
    g_test_func = test_func;
    if (!NaClSrpcModuleInit()) {
      return 1;
    }
    return !NaClSrpcAcceptClientConnection(srpc_methods);
  }
}

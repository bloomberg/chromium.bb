/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/shared/imc/nacl_imc_c.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/trusted/desc/nacl_desc_imc.h"
#include "native_client/src/shared/platform/nacl_threads.h"


#define TEST_NUM 42

/* Writes the last error message to the standard error. */
static void failWithErrno(const char* message) {
  char buffer[256];

  if (0 == NaClGetLastErrorString(buffer, sizeof(buffer))) {
    fprintf(stderr, "%s: %s", message, buffer);
  }
  exit(EXIT_FAILURE);
}

static void handleGetNum(NaClSrpcRpc* rpc,
                         NaClSrpcArg** ins,
                         NaClSrpcArg** outs,
                         NaClSrpcClosure* done) {
  UNREFERENCED_PARAMETER(ins);
  outs[0]->u.ival = TEST_NUM;
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

void WINAPI serviceThread(void* arg) {
  struct NaClDesc* desc;
  NaClSrpcHandlerDesc handlers[] = {
    { "getNum::i", handleGetNum },
    { NULL, NULL }
  };
  desc = (struct NaClDesc*) arg;

  if (!NaClSrpcServerLoop(desc, handlers, 0)) {
    failWithErrno("NaClSrpcServerLoop");
    exit(EXIT_FAILURE);
  }
  NaClDescUnref(desc);
  NaClThreadExit();
}

int main(int argc, char* argv[]) {
  int result;
  NaClHandle pair[2];
  struct NaClDescImcDesc* imc_desc[2];
  struct NaClDesc* peer[2];
  struct NaClThread thr;
  NaClSrpcChannel channel;
  NaClSrpcError error;

  UNREFERENCED_PARAMETER(argc);
  UNREFERENCED_PARAMETER(argv);

  NaClSrpcModuleInit();
  NaClNrdAllModulesInit();

  if (0 != NaClSocketPair(pair)) {
    failWithErrno("SocketPair");
  }

  imc_desc[0] = (struct NaClDescImcDesc*) calloc(1, sizeof(*imc_desc[0]));
  if (0 == imc_desc[0]) {
    failWithErrno("calloc");
  }
  imc_desc[1] = (struct NaClDescImcDesc*) calloc(1, sizeof(*imc_desc[1]));
  if (0 == imc_desc[1]) {
    failWithErrno("calloc");
  }

  if (!NaClDescImcDescCtor(imc_desc[0], pair[0])) {
    failWithErrno("NaClDescImcDescCtor");
  }

  if (!NaClDescImcDescCtor(imc_desc[1], pair[1])) {
    failWithErrno("NaClDescImcDescCtor");
  }
  peer[0] = (struct NaClDesc*) imc_desc[0];
  peer[1] = (struct NaClDesc*) imc_desc[1];

  if (!NaClThreadCreateJoinable(&thr, serviceThread, peer[0], 128*1024)) {
    failWithErrno("NaClThreadCtor");
  }

  if (!NaClSrpcClientCtor(&channel, peer[1])) {
    failWithErrno("NaClSrpcClientCtor");
  }

  error = NaClSrpcInvokeBySignature(&channel, "getNum::i", &result);
  if (NACL_SRPC_RESULT_OK != error) {
    fprintf(stderr,
            "SRPC call to getNum::i failed: %s\n",
            NaClSrpcErrorString(error));
    exit(EXIT_FAILURE);
  }

  if (TEST_NUM != result) {
    fprintf(stderr, "expected: %d, got: %d\n", TEST_NUM, result);
    exit(EXIT_FAILURE);
  }

  NaClSrpcDtor(&channel);
  NaClDescUnref(peer[1]);

  NaClThreadJoin(&thr);

  NaClSrpcModuleFini();
  return 0;
}

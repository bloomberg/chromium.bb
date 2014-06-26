/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/shared/imc/nacl_imc_c.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/shared/platform/nacl_error.h"
#include "native_client/src/shared/platform/nacl_threads.h"

#if !defined(__native_client__)
#include "native_client/src/trusted/desc/nacl_desc_imc.h"
#include "native_client/src/trusted/desc/nrd_all_modules.h"
#endif


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

struct ServiceThreadArgs {
  NaClSrpcImcDescType desc;
};

void WINAPI serviceThread(void* arg) {
  struct ServiceThreadArgs* typedArg;
  NaClSrpcImcDescType desc;
  NaClSrpcHandlerDesc handlers[] = {
    { "getNum::i", handleGetNum },
    { NULL, NULL }
  };
  typedArg = (struct ServiceThreadArgs*) arg;
  desc = typedArg->desc;
  free(typedArg);

  if (!NaClSrpcServerLoop(desc, handlers, 0)) {
    failWithErrno("NaClSrpcServerLoop");
    exit(EXIT_FAILURE);
  }
#ifdef __native_client__
  close(desc);
#else
  NaClDescUnref(desc);
#endif
  NaClThreadExit();
}

int main(int argc, char* argv[]) {
  int result;
  NaClHandle pair[2];
#ifdef __native_client__
  int imc_desc[2];
#else
  struct NaClDescImcDesc* imc_desc[2];
#endif
  struct NaClThread thr;
  NaClSrpcChannel channel;
  NaClSrpcError error;
  struct ServiceThreadArgs* threadArg;

  UNREFERENCED_PARAMETER(argc);
  UNREFERENCED_PARAMETER(argv);

  NaClSrpcModuleInit();

  if (0 != NaClSocketPair(pair)) {
    failWithErrno("SocketPair");
  }

#ifdef __native_client__
  imc_desc[0] = pair[0];
  imc_desc[1] = pair[1];
#else
  NaClNrdAllModulesInit();

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
#endif

  threadArg = (struct ServiceThreadArgs*) calloc(1, sizeof(*threadArg));
  if (NULL == threadArg) {
    failWithErrno("calloc for threadArg");
  }
  threadArg->desc = (NaClSrpcImcDescType)imc_desc[0];

  if (!NaClThreadCreateJoinable(&thr, serviceThread, threadArg, 128*1024)) {
    failWithErrno("NaClThreadCtor");
  }

  if (!NaClSrpcClientCtor(&channel, (NaClSrpcImcDescType) imc_desc[1])) {
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
#ifdef __native_client__
  close(imc_desc[1]);
#else
  NaClDescUnref((NaClSrpcImcDescType)imc_desc[1]);
#endif

  NaClThreadJoin(&thr);

  NaClSrpcModuleFini();
  return 0;
}

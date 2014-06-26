/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


#include <stdio.h>
#include <string.h>

#include "native_client/src/shared/imc/nacl_imc_c.h"
#include "native_client/src/shared/platform/nacl_error.h"
#include "native_client/src/shared/platform/nacl_threads.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"

#if !defined(__native_client__)
#include "native_client/src/trusted/desc/nacl_desc_imc.h"
#include "native_client/src/trusted/desc/nrd_all_modules.h"
#endif


#define TEST_NUM 42
#define TEST_ARRAY_LENGTH 100

/* Writes the last error message to the standard error. */
static void failWithErrno(const char* message) {
  char buffer[256];

  if (0 == NaClGetLastErrorString(buffer, sizeof(buffer))) {
    fprintf(stderr, "%s: %s", message, buffer);
  }
  exit(EXIT_FAILURE);
}

static void handlePutArr(NaClSrpcRpc* rpc,
                         NaClSrpcArg** ins,
                         NaClSrpcArg** outs,
                         NaClSrpcClosure* done) {
  int i;
  UNREFERENCED_PARAMETER(outs);

  for (i = 0; i < (int)ins[0]->u.count; i++) {
    int cur = ins[0]->arrays.carr[i];
    if (TEST_NUM != cur) {
      fprintf(stderr,
              "Wrong char at pos %d. Expected %d, has %d\n",
              i,
              TEST_NUM,
              cur);
      exit(EXIT_FAILURE);
    }
  }

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
    { "putArr:C:", handlePutArr },
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
  nacl_abi_size_t char_array_count;
  char* char_array_carr;
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

  char_array_count = TEST_ARRAY_LENGTH;
  char_array_carr = (char*) calloc(TEST_ARRAY_LENGTH, sizeof(char));
  if (!char_array_carr) {
    failWithErrno("calloc for char_array");
  }
  memset(char_array_carr, TEST_NUM, TEST_ARRAY_LENGTH);

  error = NaClSrpcInvokeBySignature(&channel,
                                    "putArr:C:",
                                    char_array_count,
                                    char_array_carr);

  if (NACL_SRPC_RESULT_OK != error) {
    fprintf(stderr,
            "SRPC call to putArr:C: failed: %s\n",
            NaClSrpcErrorString(error));
    exit(EXIT_FAILURE);
  }
  free(char_array_carr);

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

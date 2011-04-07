// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Note: this file defines hooks for all pepper related srpc calls
// it would be nice to keep this synchronized with
// src/shared/ppapi_proxy/ppb_rpc_server.cc
// which is a generated file
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/platform/nacl_threads.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/desc/nacl_desc_imc_shm.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/sel_universal/pepper_handler.h"
#include "native_client/src/trusted/sel_universal/rpc_universal.h"

#include <stdio.h>
#include <string>

using nacl::DescWrapperFactory;
using nacl::DescWrapper;


namespace {

#define SRPC_STD_ARGS NaClSrpcRpc* rpc, \
                      NaClSrpcArg** inputs, \
                      NaClSrpcArg** outputs, \
                      NaClSrpcClosure* done


void Unimplemented(SRPC_STD_ARGS) {
  UNREFERENCED_PARAMETER(inputs);
  UNREFERENCED_PARAMETER(outputs);
  UNREFERENCED_PARAMETER(done);

  const char* rpc_name;
  const char* arg_types;
  const char* ret_types;

  if (NaClSrpcServiceMethodNameAndTypes(rpc->channel->server,
                                        rpc->rpc_number,
                                        &rpc_name,
                                        &arg_types,
                                        &ret_types)) {
    NaClLog(LOG_ERROR, "cannot find signature for rpc %d\n", rpc->rpc_number);
  }

  // TODO(robertm): add full argument printing
  printf("invoking: %s (%s) -> %s\n", rpc_name, arg_types, ret_types);
}


NaClSrpcHandlerDesc srpc_methods[] = {
  {"Dummy::", &Unimplemented},
  { NULL, NULL }
};


void WINAPI PepperHandlerThread(void* desc_void) {
  DescWrapper* desc = reinterpret_cast<DescWrapper*>(desc_void);

  NaClLog(1, "pepper secondary service started %p\n", desc_void);
  NaClSrpcServerLoop(desc->desc(), srpc_methods, NULL);
  NaClLog(1, "pepper secondary service stopped\n");
  NaClThreadExit();
}

}  // end namespace

bool HandlerPepperInit(NaClCommandLoop* ncl, const vector<string>& args) {
  if (args.size() != 2) {
    NaClLog(LOG_ERROR, "not the right number of args for this command\n");
    return false;
  }

  DescWrapperFactory* factory = new DescWrapperFactory();
  NaClLog(1, "InitializePepperRpc\n");

  // NOTE: these are really NaClDescXferableDataDesc. Code was mimicked after
  //       the exisiting plugin code
  NaClLog(1, "create socket pair so that client can make pepper upcalls\n");

  DescWrapper* descs[2] = { NULL, NULL };
  if (0 != factory->MakeSocketPair(descs)) {
    NaClLog(LOG_FATAL, "cannot create socket pair\n");
  }

  NaClLog(1, "spawning secondary service thread\n");

  NaClThread thread;
  if (!NaClThreadCtor(
        &thread,
        PepperHandlerThread,
        descs[0],
        128 << 10)) {
    NaClLog(LOG_FATAL, "cannot create service handler thread\n");
  }

  ncl->AddDesc(descs[1]->desc(), args[1]);
  return true;
}

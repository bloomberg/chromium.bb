/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/reverse_service/reverse_service.h"

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"

namespace {

void Test(struct NaClSrpcRpc *rpc,
          struct NaClSrpcArg **in_args,
          struct NaClSrpcArg **out_args,
          struct NaClSrpcClosure *done) {
  char *msg = in_args[0]->arrays.str;
  UNREFERENCED_PARAMETER(out_args);
  // use rpc->channel rather than rpc->channel->server_instance_data
  // to show that Test RPCs arrive in different channels.
  NaClLog(0, "Test: [%"NACL_PRIxPTR"] %s\n",
          reinterpret_cast<uintptr_t>(rpc->channel), msg);
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

void AddChannel(struct NaClSrpcRpc *rpc,
                struct NaClSrpcArg **in_args,
                struct NaClSrpcArg **out_args,
                struct NaClSrpcClosure *done) {
  nacl::ReverseService* service = reinterpret_cast<nacl::ReverseService*>(
      rpc->channel->server_instance_data);
  UNREFERENCED_PARAMETER(in_args);
  UNREFERENCED_PARAMETER(out_args);
  NaClLog(4, "Entered AddChannel\n");
  service->Start(reinterpret_cast<void*>(service));
  NaClLog(4, "Leaving AddChannel\n");
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

}  // namespace

namespace nacl {

NaClSrpcHandlerDesc ReverseService::handlers[] = {
  { "test:s:", Test, },
  { "add_channel::", AddChannel, },
  { NULL, NULL, },
};

ReverseService::ReverseService(nacl::DescWrapper* conn_cap)
    : service_socket_(conn_cap, handlers) {
  NaClLog(4, "ReverseService::ReverseService ctor invoked\n");
}

ReverseService::~ReverseService() {}

bool ReverseService::Start(void* server_instance_data) {
  return service_socket_.StartService(server_instance_data);
}

}  // namespace nacl

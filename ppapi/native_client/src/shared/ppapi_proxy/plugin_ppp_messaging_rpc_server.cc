// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/ppapi_proxy/object_serialize.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "ppapi/c/ppp.h"
#include "ppapi/c/ppp_messaging.h"
#include "srpcgen/ppp_rpc.h"

using ppapi_proxy::DebugPrintf;
using ppapi_proxy::DeserializeTo;
using ppapi_proxy::PPPMessagingInterface;

void PppMessagingRpcServer::PPP_Messaging_HandleMessage(
      NaClSrpcRpc* rpc,
      NaClSrpcClosure* done,
      PP_Instance instance,
      nacl_abi_size_t message_size, char* message_bytes) {
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  NaClSrpcClosureRunner runner(done);

  PP_Var message;
  if (!DeserializeTo(rpc->channel, message_bytes, message_size, 1, &message))
    return;
  PPPMessagingInterface()->HandleMessage(instance, message);
  DebugPrintf("PPP_Messaging::HandleMessage\n");
  rpc->result = NACL_SRPC_RESULT_OK;
}

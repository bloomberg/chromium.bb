// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/ppapi_proxy/object_serialize.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "srpcgen/ppb_rpc.h"

// This is here due to a Windows API collision.
#ifdef PostMessage
#undef PostMessage
#endif

using ppapi_proxy::PPBMessagingInterface;
using ppapi_proxy::DebugPrintf;
using ppapi_proxy::DeserializeTo;

void PpbMessagingRpcServer::PPB_Messaging_PostMessage(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Instance instance,
    nacl_abi_size_t message_size,
    char* message_bytes) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_Var message;
  if (!DeserializeTo(rpc->channel, message_bytes, message_size, 1, &message))
    return;

  PPBMessagingInterface()->PostMessage(instance, message);
  DebugPrintf("PPB_Messaging::PostMessage: instance=%"NACL_PRIu32"\n",
              instance);

  rpc->result = NACL_SRPC_RESULT_OK;
}


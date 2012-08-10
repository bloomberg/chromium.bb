// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SRPC-abstraction wrappers around PPB_NetworkMonitor_Private functions.

#include <string.h>
#include <limits>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/ppapi_proxy/browser_callback.h"
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_udp_socket_private.h"
#include "srpcgen/ppb_rpc.h"
#include "srpcgen/ppp_rpc.h"

using ppapi_proxy::DebugPrintf;
using ppapi_proxy::DeleteRemoteCallbackInfo;
using ppapi_proxy::MakeRemoteCompletionCallback;
using ppapi_proxy::PPBNetworkMonitorPrivateInterface;

namespace {

struct NetworkListCallbackData {
  PP_Instance instance_id;
  PP_Resource network_monitor;
};

void NetworkListChangedCallback(void* user_data, PP_Resource network_list) {
  NetworkListCallbackData* data =
      reinterpret_cast<NetworkListCallbackData*>(user_data);

  NaClSrpcChannel* nacl_srpc_channel = ppapi_proxy::GetMainSrpcChannel(
      data->instance_id);
  PppNetworkMonitorPrivateRpcClient::
      PPP_NetworkMonitor_Private_NetworkListChanged(
          nacl_srpc_channel, data->network_monitor, network_list);
}

}  // namespace

void PpbNetworkMonitorPrivateServer::PPB_NetworkMonitor_Private_Create(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Instance instance,
    PP_Resource* resource) {
  DebugPrintf("PPB_NetworkMonitor_Private::Create: "
              "instance=%"NACL_PRId32"\n", instance);

  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  // NetworkListCallbackData is always leaked - it's non-trivial to detect when
  // NetworkMonitor resource is destroyed. This code is going to be removed
  // soon, so it doesn't make sense to fix it.
  //
  // TODO(sergeyu): Fix this leak if for any reason we decide to keep this code
  // long term.
  NetworkListCallbackData* callback_data = new NetworkListCallbackData();
  callback_data->instance_id = instance;

  *resource = PPBNetworkMonitorPrivateInterface()->Create(
      instance, &NetworkListChangedCallback, callback_data);

  callback_data->network_monitor = *resource;

  rpc->result = NACL_SRPC_RESULT_OK;
}

void
PpbNetworkMonitorPrivateServer::PPB_NetworkMonitor_Private_IsNetworkMonitor(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource resource,
    int32_t* out_bool) {
  DebugPrintf("PPB_NetworkMonitor_Private::IsNetworkMonitor: "
              "resource=%"NACL_PRId32"\n", resource);

  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_Bool pp_success =
      PPBNetworkMonitorPrivateInterface()->IsNetworkMonitor(resource);
  *out_bool = PP_ToBool(pp_success);

  rpc->result = NACL_SRPC_RESULT_OK;
}

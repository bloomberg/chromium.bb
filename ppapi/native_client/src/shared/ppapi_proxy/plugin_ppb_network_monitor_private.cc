// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/plugin_ppb_network_monitor_private.h"

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/plugin_callback.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_network_monitor_private.h"
#include "srpcgen/ppb_rpc.h"
#include "srpcgen/ppp_rpc.h"

namespace ppapi_proxy {

PluginNetworkMonitorPrivate::PluginNetworkMonitorPrivate()
    : network_list_callback_(NULL),
      user_data_(NULL) {
}

PluginNetworkMonitorPrivate::~PluginNetworkMonitorPrivate() {
}

void PluginNetworkMonitorPrivate::OnNetworkListChanged(
    PP_Resource list_resource) {
  DCHECK(network_list_callback_);
  network_list_callback_(user_data_, list_resource);
}


namespace {

PP_Resource Create(PP_Instance instance_id,
                   PPB_NetworkMonitor_Callback callback,
                   void* user_data) {
  DebugPrintf("PPB_NetworkMonitor_Private::Create: "
              "instance_id=%"NACL_PRId32"\n", instance_id);

  if (NULL == callback)
    return kInvalidResourceId;

  PP_Resource resource;
  NaClSrpcError srpc_result =
      PpbNetworkMonitorPrivateClient::PPB_NetworkMonitor_Private_Create(
          GetMainSrpcChannel(), instance_id, &resource);

  DebugPrintf("PPB_NetworkMonitor_Private::Create: %d %s\n",
              resource, NaClSrpcErrorString(srpc_result));

  if (srpc_result != NACL_SRPC_RESULT_OK)
    return kInvalidResourceId;

  scoped_refptr<PluginNetworkMonitorPrivate> network_monitor =
      PluginResource::AdoptAs<PluginNetworkMonitorPrivate>(resource);
  if (!network_monitor.get())
    return kInvalidResourceId;

  network_monitor->set_callback(callback, user_data);

  return resource;
}

PP_Bool IsNetworkMonitor(PP_Resource resource_id) {
  DebugPrintf("PPB_NetworkMonitor_Private::IsNetworkMonitor: "
              "resource_id=%"NACL_PRId32"\n", resource_id);

  int32_t is_network_monitor;
  NaClSrpcError srpc_result = PpbNetworkMonitorPrivateClient::
      PPB_NetworkMonitor_Private_IsNetworkMonitor(
          GetMainSrpcChannel(), resource_id, &is_network_monitor);

  DebugPrintf("PPB_NetworkMonitor_Private::IsNetworkMonitor: %s\n",
              NaClSrpcErrorString(srpc_result));

  if (srpc_result != NACL_SRPC_RESULT_OK)
    return PP_FALSE;

  return PP_FromBool(is_network_monitor);
}

}  // namespace

const PPB_NetworkMonitor_Private*
PluginNetworkMonitorPrivate::GetInterface() {
  static const PPB_NetworkMonitor_Private networkmonitor_private_interface = {
    Create,
    IsNetworkMonitor,
  };
  return &networkmonitor_private_interface;
}

bool PluginNetworkMonitorPrivate::InitFromBrowserResource(
    PP_Resource resource) {
  DebugPrintf("PluginNetworkMonitorPrivate::InitFromBrowserResource: "
              "resource=%"NACL_PRId32"\n", resource);
  resource_ = resource;
  return true;
}

}  // namespace ppapi_proxy

void
PppNetworkMonitorPrivateRpcServer::
PPP_NetworkMonitor_Private_NetworkListChanged(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Resource network_monitor_id,
    PP_Resource network_list) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_OK;
  ppapi_proxy::DebugPrintf(
      "PPP_NetworkMonitor_Private::NetworkListChanged: "
      "network_monitor_id=%"NACL_PRId32" network_list=%"NACL_PRId32"\n",
      network_monitor_id, network_list);
  scoped_refptr<ppapi_proxy::PluginNetworkMonitorPrivate> network_monitor =
      ppapi_proxy::PluginResource::
      GetAs<ppapi_proxy::PluginNetworkMonitorPrivate>(network_monitor_id);

  if (!network_monitor.get())
    return;

  network_monitor->OnNetworkListChanged(network_list);
}

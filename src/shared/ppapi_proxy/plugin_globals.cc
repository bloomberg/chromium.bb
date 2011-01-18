// Copyright (c) 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/plugin_core.h"
#include "native_client/src/shared/ppapi_proxy/plugin_var.h"
#include "native_client/src/shared/ppapi_proxy/ppruntime.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "srpcgen/ppp_rpc.h"

namespace ppapi_proxy {

namespace {

NaClSrpcChannel* main_srpc_channel;
NaClSrpcChannel* upcall_srpc_channel;
PP_Module module_id_for_plugin;

}  // namespace;

const PP_Resource kInvalidResourceId = 0;

NaClSrpcChannel* GetMainSrpcChannel() {
  return main_srpc_channel;
}

void SetMainSrpcChannel(NaClSrpcChannel* channel) {
  main_srpc_channel = channel;
}

NaClSrpcChannel* GetUpcallSrpcChannel() {
  return upcall_srpc_channel;
}

void SetUpcallSrpcChannel(NaClSrpcChannel* channel) {
  upcall_srpc_channel = channel;
}

void SetModuleIdForSrpcChannel(NaClSrpcChannel* channel, PP_Module module_id) {
  module_id_for_plugin = module_id;
}

void UnsetModuleIdForSrpcChannel(NaClSrpcChannel* channel) {
  const PP_Module kInvalidModuleId = 0;
  module_id_for_plugin = kInvalidModuleId;
}

PP_Module LookupModuleIdForSrpcChannel(NaClSrpcChannel* channel) {
  return module_id_for_plugin;
}

const void* GetBrowserInterfaceSafe(const char* interface_name) {
  const void* ppb_interface = GetBrowserInterface(interface_name);
  if (ppb_interface == NULL)
    DebugPrintf("PPB_GetInterface: %s not found\n", interface_name);
  CHECK(ppb_interface);
  return ppb_interface;
}

const PPB_Core* PPBCoreInterface() {
  return reinterpret_cast<const PPB_Core*>(
      GetBrowserInterfaceSafe(PPB_CORE_INTERFACE));
}

const PPB_Var_Deprecated* PPBVarInterface() {
  return reinterpret_cast<const PPB_Var_Deprecated*>(
      GetBrowserInterfaceSafe(PPB_VAR_DEPRECATED_INTERFACE));
}

int PluginMain() {
  if (!NaClSrpcModuleInit()) {
    return 1;
  }
  NaClLogModuleInit();  // Enable NaClLog'ing used by CHECK().
  // Designate this as the main thread for PPB_Core::IsMainThread().
  ppapi_proxy::PluginCore::MarkMainThread();
  if (!NaClSrpcAcceptClientConnection(PppRpcs::srpc_methods)) {
    return 1;
  }
  NaClSrpcModuleFini();

  return 0;
}

}  // namespace ppapi_proxy

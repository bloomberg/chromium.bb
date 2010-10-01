// Copyright (c) 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/plugin_core.h"
#include "native_client/src/shared/ppapi_proxy/plugin_var.h"

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

const PPB_Core* CoreInterface() {
  return reinterpret_cast<const PPB_Core*>(PluginCore::GetInterface());
}

const PPB_Var* VarInterface() {
  return PluginVar::GetInterface();
}

}  // namespace ppapi_proxy

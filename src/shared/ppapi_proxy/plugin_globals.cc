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

}  // namespace;

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

const PPB_Core* CoreInterface() {
  return reinterpret_cast<const PPB_Core*>(PluginCore::GetInterface());
}

const PPB_Var* VarInterface() {
  return PluginVar::GetInterface();
}

}  // namespace ppapi_proxy

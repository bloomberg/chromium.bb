// Copyright (c) 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_GLOBALS_H_
#define NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_GLOBALS_H_

#include "ppapi/c/pp_module.h"
#include "ppapi/c/ppb.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_var.h"

struct NaClSrpcChannel;

namespace ppapi_proxy {

// The main SRPC channel is that used to handle foreground (main thread)
// RPC traffic.
NaClSrpcChannel* GetMainSrpcChannel();
void SetMainSrpcChannel(NaClSrpcChannel* channel);

// The upcall SRPC channel is that used to handle other threads' RPC traffic.
NaClSrpcChannel* GetUpcallSrpcChannel();
void SetUpcallSrpcChannel(NaClSrpcChannel* channel);

// Save the plugin's module_id, which is used for storage allocation tracking.
void SetModuleIdForSrpcChannel(NaClSrpcChannel* channel, PP_Module module_id);
// Forget the plugin's module_id.
void UnsetModuleIdForSrpcChannel(NaClSrpcChannel* channel);
// Save the plugin's module_id.
PP_Module LookupModuleIdForSrpcChannel(NaClSrpcChannel* channel);

// Get the PPB_Core interface passed in from the browser.
const PPB_Core* CoreInterface();

// Get the PPB_Var interface passed in from the browser.
const PPB_Var* VarInterface();

}  // namespace ppapi_proxy

#endif  // NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_GLOBALS_H_

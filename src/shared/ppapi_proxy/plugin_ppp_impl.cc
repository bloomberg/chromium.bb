// Copyright (c) 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdarg.h>

#include "native_client/src/include/portability.h"
#include "native_client/src/include/portability_process.h"
#include "gen/native_client/src/shared/ppapi_proxy/ppp_rpc.h"
#include "native_client/src/shared/ppapi_proxy/plugin_getinterface.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "ppapi/c/ppp.h"

using ppapi_proxy::DebugPrintf;

namespace {

// The plugin will make synchronous calls back to the browser on the main
// thread.  The service exported from the browser is specified in
// service_description.
bool StartMainSrpcChannel(const char* service_description,
                          NaClSrpcChannel* channel) {
  NaClSrpcService* service =
      reinterpret_cast<NaClSrpcService*>(calloc(1, sizeof(*service)));
  if (NULL == service) {
    return false;
  }
  if (!NaClSrpcServiceStringCtor(service, service_description)) {
    free(service);
    return false;
  }
  channel->client = service;
  // Remember the main channel for later calls back from the main thread.
  ppapi_proxy::SetMainSrpcChannel(channel);
  return true;
}

void StopMainSrpcChannel() {
  NaClSrpcChannel* channel = ppapi_proxy::GetMainSrpcChannel();
  NaClSrpcService* service = channel->client;
  // Invoke the dtor on the exported service.
  NaClSrpcServiceDtor(service);
  // Remove the service from the channel.
  channel->client = NULL;
  // Forget the main channel.
  ppapi_proxy::SetMainSrpcChannel(NULL);
}

// The plugin will make asynchronous calls to the browser on other threads.
// The service exported on this channel will be gotten by service discovery.
bool StartUpcallSrpcChannel(NaClSrpcImcDescType upcall_channel_desc) {
  // Create the upcall srpc client.
  if (upcall_channel_desc == -1) {
    DebugPrintf("  Bad upcall_channel_desc\n");
    return false;
  }
  NaClSrpcChannel* upcall_channel = reinterpret_cast<NaClSrpcChannel*>(
      calloc(1, sizeof(*upcall_channel)));
  if (NULL == upcall_channel) {
    return false;
  }
  if (!NaClSrpcClientCtor(upcall_channel, upcall_channel_desc)) {
    free(upcall_channel);
    return false;
  }
  // Remember the upcall channel client.
  ppapi_proxy::SetUpcallSrpcChannel(upcall_channel);
  return true;
}

void StopUpcallSrpcChannel() {
  NaClSrpcChannel* channel = ppapi_proxy::GetUpcallSrpcChannel();
  // Invoke the dtor on the upcall channel
  NaClSrpcDtor(channel);
  // Forget the upcall channel.
  ppapi_proxy::SetUpcallSrpcChannel(NULL);
}

}  // namespace

//
// The following methods are the SRPC dispatchers for ppapi/c/ppp.h.
//

NaClSrpcError PppRpcServer::PPP_InitializeModule(
    NaClSrpcChannel *channel,
    int32_t pid,
    int64_t module,
    NaClSrpcImcDescType upcall_channel_desc,
    char* service_description,
    int32_t* nacl_pid,
    int32_t* success) {
  DebugPrintf("PPP_InitializeModule: %s\n", service_description);
  // Set up the service for calling back into the browser.
  if (!StartMainSrpcChannel(const_cast<const char*>(service_description),
                            channel)) {
    DebugPrintf("  Failed to export service on main channel\n");
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  // Set up the upcall channel for calling back into the browser.
  if (!StartUpcallSrpcChannel(upcall_channel_desc)) {
    DebugPrintf("  Failed to construct upcall channel\n");
    StopMainSrpcChannel();
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  *success = ::PPP_InitializeModule(module, ppapi_proxy::GetInterfaceProxy);
  *nacl_pid = GETPID();
  return NACL_SRPC_RESULT_OK;
}

NaClSrpcError PppRpcServer::PPP_ShutdownModule(NaClSrpcChannel* channel) {
  DebugPrintf("PPP_ShutdownModule\n");
  ::PPP_ShutdownModule();
  // Clean up the upcall channel.
  StopUpcallSrpcChannel();
  // Clean up the main channel.
  StopMainSrpcChannel();
  return NACL_SRPC_RESULT_OK;
}

NaClSrpcError PppRpcServer::PPP_GetInterface(NaClSrpcChannel* channel,
                                             char* interface_name,
                                             int32_t* exports_interface_name) {
  DebugPrintf("PPP_GetInterface(%s)\n", interface_name);
  // Since the proxy will make calls to proxied interfaces, we need simply
  // to know whether the plugin exports a given interface.
  const void* plugin_interface = ::PPP_GetInterface(interface_name);
  *exports_interface_name = (plugin_interface != NULL);
  return NACL_SRPC_RESULT_OK;
}

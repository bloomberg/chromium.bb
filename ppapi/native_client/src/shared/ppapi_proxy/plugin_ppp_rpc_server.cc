// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SRPC-abstraction wrappers around PPP functions.

#include <stdarg.h>

#include "native_client/src/include/portability.h"
#include "native_client/src/include/portability_process.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "ppapi/c/ppp.h"
#include "srpcgen/ppp_rpc.h"

using ppapi_proxy::DebugPrintf;

namespace {

const int kInvalidDesc = -1;

// In order to close the upcall socket descriptor when we shut down, we
// need to remember it.
// TODO(sehr,polina): This ugly state should be attached to the correct
// data structure for the plugin module.
int upcall_socket_fd = kInvalidDesc;

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
  NaClSrpcServiceDtor(service);
  channel->client = NULL;
  ppapi_proxy::SetMainSrpcChannel(NULL);
}

// The plugin will make asynchronous calls to the browser on other threads.
// The service exported on this channel will be gotten by service discovery.
bool StartUpcallSrpcChannel(NaClSrpcImcDescType upcall_channel_desc) {
  // Create the upcall srpc client.
  if (upcall_channel_desc == kInvalidDesc) {
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
  ppapi_proxy::SetUpcallSrpcChannel(upcall_channel);
  upcall_socket_fd = upcall_channel_desc;
  return true;
}

void StopUpcallSrpcChannel() {
  NaClSrpcChannel* upcall_channel = ppapi_proxy::GetUpcallSrpcChannel();
  NaClSrpcDtor(upcall_channel);
  ppapi_proxy::SetUpcallSrpcChannel(NULL);
  close(upcall_socket_fd);
  upcall_socket_fd = kInvalidDesc;
}

}  // namespace

//
// The following methods are the SRPC dispatchers for ppapi/c/ppp.h.
//

void PppRpcServer::PPP_InitializeModule(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    int32_t pid,
    PP_Module module,
    NaClSrpcImcDescType upcall_channel_desc,
    char* service_description,
    int32_t* nacl_pid,
    int32_t* success) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  DebugPrintf("PPP_InitializeModule: module=%"NACL_PRIu32": %s\n",
              module, service_description);
  // Set up the service for calling back into the browser.
  if (!StartMainSrpcChannel(const_cast<const char*>(service_description),
                            rpc->channel)) {
    DebugPrintf("PPP_InitializeModule: "
                "failed to export service on main channel\n");
    return;
  }
  // Set up the upcall channel for calling back into the browser.
  if (!StartUpcallSrpcChannel(upcall_channel_desc)) {
    DebugPrintf("PPP_InitializeModule: "
                "failed to construct upcall channel\n");
    StopMainSrpcChannel();
    return;
  }
  ppapi_proxy::SetModuleIdForSrpcChannel(rpc->channel, module);
  *success = ::PPP_InitializeModule(module, ppapi_proxy::GetBrowserInterface);
  *nacl_pid = GETPID();
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PppRpcServer::PPP_ShutdownModule(NaClSrpcRpc* rpc,
                                      NaClSrpcClosure* done) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  DebugPrintf("PPP_ShutdownModule\n");
  // We do NOT call ::PPP_ShutdownModule as it cannot do anything useful
  // at this point while still having an ability to hang the browser.
  ppapi_proxy::UnsetModuleIdForSrpcChannel(rpc->channel);
  StopUpcallSrpcChannel();
  // TODO(sehr, polina): do we even need this function?
  // Shouldn't the Dtor be called when nexe's main exits?
  //StopMainSrpcChannel();
  // Exit the srpc loop. The server won't answer any more requests.
  rpc->result = NACL_SRPC_RESULT_BREAK;
  DebugPrintf("PPP_ShutdownModule: %s\n", NaClSrpcErrorString(rpc->result));
}

void PppRpcServer::PPP_GetInterface(NaClSrpcRpc* rpc,
                                    NaClSrpcClosure* done,
                                    char* interface_name,
                                    int32_t* exports_interface_name) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  DebugPrintf("PPP_GetInterface('%s')\n", interface_name);
  // Since the proxy will make calls to proxied interfaces, we need simply
  // to know whether the plugin exports a given interface.
  const void* plugin_interface = ::PPP_GetInterface(interface_name);
  *exports_interface_name = (plugin_interface != NULL);
  rpc->result = NACL_SRPC_RESULT_OK;
}

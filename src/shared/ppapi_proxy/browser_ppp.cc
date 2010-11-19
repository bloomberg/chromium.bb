// Copyright (c) 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/browser_ppp.h"

#include <string.h>

#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/include/portability_process.h"
#include "srpcgen/ppb_rpc.h"
#include "srpcgen/ppp_rpc.h"
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/ppapi_proxy/browser_instance.h"
#include "native_client/src/shared/ppapi_proxy/browser_upcall.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppp.h"

namespace ppapi_proxy {

//
// The following methods are the SRPC dispatchers for ppapi/c/ppp.h.
//

int32_t BrowserPpp::InitializeModule(
    PP_Module module_id,
    PPB_GetInterface get_browser_interface,
    PP_Instance instance) {
  DebugPrintf("BrowserPpp::InitializeModule(%"NACL_PRIu64", %p)\n",
              module_id, get_browser_interface);
  SetBrowserGetInterface(get_browser_interface);
  SetBrowserPppForInstance(instance, this);
  nacl::scoped_ptr<nacl::DescWrapper> wrapper(
      BrowserUpcall::Start(&upcall_thread_, channel_));
  if (wrapper.get() == NULL) {
    return PP_ERROR_FAILED;
  }
  // Set up the callbacks allowed on the main channel.
  NaClSrpcService* service = reinterpret_cast<NaClSrpcService*>(
      calloc(1, sizeof(*service)));
  if (NULL == service) {
    DebugPrintf("Couldn't create callback services.\n");
    return PP_ERROR_FAILED;
  }
  if (!NaClSrpcServiceHandlerCtor(service, PpbRpcs::srpc_methods)) {
    DebugPrintf("Couldn't construct callback services.\n");
    free(service);
    return PP_ERROR_FAILED;
  }
  // Export the service on the channel.
  channel_->server = service;
  char* service_string = const_cast<char*>(service->service_string);
  SetModuleIdForSrpcChannel(channel_, module_id);
  // Do the RPC.
  int32_t browser_pid = static_cast<int32_t>(GETPID());
  int32_t success;
  NaClSrpcError retval =
      PppRpcClient::PPP_InitializeModule(channel_,
                                         browser_pid,
                                         module_id,
                                         wrapper->desc(),
                                         service_string,
                                         &plugin_pid_,
                                         &success);
  if (retval != NACL_SRPC_RESULT_OK) {
    DebugPrintf("InitializeModule failed %02x\n", retval);
    return PP_ERROR_FAILED;
  }
  DebugPrintf("InitializeModule succeeded %02x\n", success);
  return success;
}

void BrowserPpp::ShutdownModule() {
  DebugPrintf("BrowserPpp::ShutdownModule\n");
  PppRpcClient::PPP_ShutdownModule(channel_);
  NaClThreadJoin(&upcall_thread_);
  UnsetModuleIdForSrpcChannel(channel_);
}

const void* BrowserPpp::GetInterface(const char* interface_name) {
  DebugPrintf("BrowserPpp::GetInterface('%s')\n", interface_name);
  int32_t exports_interface_name;
  NaClSrpcError retval =
      PppRpcClient::PPP_GetInterface(channel_,
                                     const_cast<char*>(interface_name),
                                     &exports_interface_name);
  if (retval != NACL_SRPC_RESULT_OK || !exports_interface_name) {
    DebugPrintf("    Interface '%s' not supported\n", interface_name);
    return NULL;
  }
  if (strcmp(interface_name, PPP_INSTANCE_INTERFACE) == 0) {
    DebugPrintf("    Interface '%s' proxied\n", interface_name);
    return reinterpret_cast<const void*>(BrowserInstance::GetInterface());
  }
  // TODO(sehr): other interfaces go here.
  return NULL;
}

}  // namespace ppapi_proxy

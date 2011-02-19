// Copyright (c) 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/browser_ppp.h"

#include <string.h>

#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/include/portability_process.h"
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/ppapi_proxy/browser_ppp_instance.h"
#include "native_client/src/shared/ppapi_proxy/browser_upcall.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/plugin/ppapi/plugin_ppapi.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppp.h"
#include "srpcgen/ppb_rpc.h"
#include "srpcgen/ppp_rpc.h"

namespace ppapi_proxy {

//
// The following methods are the SRPC dispatchers for ppapi/c/ppp.h.
//

int32_t BrowserPpp::InitializeModule(
    PP_Module module_id,
    PPB_GetInterface get_browser_interface) {
  DebugPrintf("PPP_InitializeModule: module=%"NACL_PRIu32"\n", module_id);
  SetPPBGetInterface(get_browser_interface);
  SetBrowserPppForInstance(plugin_->pp_instance(), this);
  nacl::scoped_ptr<nacl::DescWrapper> wrapper(
      BrowserUpcall::Start(&upcall_thread_, main_channel_));
  if (wrapper.get() == NULL) {
    return PP_ERROR_FAILED;
  }
  // Set up the callbacks allowed on the main channel.
  NaClSrpcService* service = reinterpret_cast<NaClSrpcService*>(
      calloc(1, sizeof(*service)));
  if (NULL == service) {
    DebugPrintf("PPP_InitializeModule: "
                "could not create callback services.\n");
    return PP_ERROR_FAILED;
  }
  if (!NaClSrpcServiceHandlerCtor(service, PpbRpcs::srpc_methods)) {
    DebugPrintf("PPP_InitializeModule: "
                "could not construct callback services.\n");
    free(service);
    return PP_ERROR_FAILED;
  }
  // Export the service on the channel.
  main_channel_->server = service;
  char* service_string = const_cast<char*>(service->service_string);
  SetModuleIdForSrpcChannel(main_channel_, module_id);
  // Do the RPC.
  int32_t browser_pid = static_cast<int32_t>(GETPID());
  int32_t pp_error;
  NaClSrpcError srpc_result =
      PppRpcClient::PPP_InitializeModule(main_channel_,
                                         browser_pid,
                                         module_id,
                                         wrapper->desc(),
                                         service_string,
                                         &plugin_pid_,
                                         &pp_error);
  DebugPrintf("PPP_InitializeModule: %s\n", NaClSrpcErrorString(srpc_result));
  if (srpc_result != NACL_SRPC_RESULT_OK) {
    return PP_ERROR_FAILED;
  }
  ppp_instance_interface_ = reinterpret_cast<const PPP_Instance*>(
      GetPluginInterface(PPP_INSTANCE_INTERFACE));
  if (ppp_instance_interface_ == NULL) {
    return PP_ERROR_FAILED;
  }
  DebugPrintf("PPP_InitializeModule: pp_error=%"NACL_PRId32"\n", pp_error);
  return pp_error;
}

void BrowserPpp::ShutdownModule() {
  DebugPrintf("PPP_ShutdownModule\n");
  NaClSrpcError srpc_result =
      PppRpcClient::PPP_ShutdownModule(main_channel_);
  DebugPrintf("PPP_ShutdownModule: %s\n", NaClSrpcErrorString(srpc_result));
  NaClThreadJoin(&upcall_thread_);
  UnsetModuleIdForSrpcChannel(main_channel_);
}

const void* BrowserPpp::GetPluginInterface(const char* interface_name) {
  DebugPrintf("PPP_GetInterface('%s')\n", interface_name);
  int32_t exports_interface_name;
  NaClSrpcError srpc_result =
      PppRpcClient::PPP_GetInterface(main_channel_,
                                     const_cast<char*>(interface_name),
                                     &exports_interface_name);
  DebugPrintf("PPP_GetInterface('%s'): %s\n",
              interface_name, NaClSrpcErrorString(srpc_result));
  const void* ppp_interface = NULL;
  if (srpc_result != NACL_SRPC_RESULT_OK || !exports_interface_name) {
    ppp_interface = NULL;
  } else if (strcmp(interface_name, PPP_INSTANCE_INTERFACE) == 0) {
    ppp_interface =
       reinterpret_cast<const void*>(BrowserInstance::GetInterface());
  }
  // TODO(sehr): other interfaces go here.
  DebugPrintf("PPP_GetInterface('%s'): %p\n", interface_name, ppp_interface);
  return ppp_interface;
}

const void* BrowserPpp::GetPluginInterfaceSafe(const char* interface_name) {
  const void* ppp_interface = GetPluginInterface(interface_name);
  if (ppp_interface == NULL)
    DebugPrintf("PPB_GetInterface: %s not found\n", interface_name);
  CHECK(ppp_interface != NULL);
  return ppp_interface;
}

}  // namespace ppapi_proxy

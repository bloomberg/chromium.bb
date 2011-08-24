// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/browser_ppp.h"

#include <string.h>

#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/include/portability_process.h"
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/ppapi_proxy/browser_ppp_find.h"
#include "native_client/src/shared/ppapi_proxy/browser_ppp_input_event.h"
#include "native_client/src/shared/ppapi_proxy/browser_ppp_instance.h"
#include "native_client/src/shared/ppapi_proxy/browser_ppp_messaging.h"
#include "native_client/src/shared/ppapi_proxy/browser_ppp_printing.h"
#include "native_client/src/shared/ppapi_proxy/browser_ppp_scrollbar.h"
#include "native_client/src/shared/ppapi_proxy/browser_ppp_selection.h"
#include "native_client/src/shared/ppapi_proxy/browser_ppp_widget.h"
#include "native_client/src/shared/ppapi_proxy/browser_ppp_zoom.h"
#include "native_client/src/shared/ppapi_proxy/browser_upcall.h"
#include "native_client/src/shared/ppapi_proxy/trusted/srpcgen/ppb_rpc.h"
#include "native_client/src/shared/ppapi_proxy/trusted/srpcgen/ppp_rpc.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/plugin/plugin.h"
#include "ppapi/c/ppp_input_event.h"
#include "ppapi/c/dev/ppp_find_dev.h"
#include "ppapi/c/dev/ppp_printing_dev.h"
#include "ppapi/c/dev/ppp_scrollbar_dev.h"
#include "ppapi/c/dev/ppp_selection_dev.h"
#include "ppapi/c/dev/ppp_widget_dev.h"
#include "ppapi/c/dev/ppp_zoom_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppp.h"

namespace ppapi_proxy {

//
// The following methods are the SRPC dispatchers for ppapi/c/ppp.h.
//

namespace {

// PPB_GetInterface must only be called on the main thread. So as we are adding
// off-the-main-thread support for various interfaces, we need to ensure that
// their pointers are available on upcall thread of the the trusted proxy.
void PPBGetInterfaces() {
  PPBCoreInterface();
  // TODO(all): add more interfaces here once available off the main thread.
}

}  // namespace

int32_t BrowserPpp::InitializeModule(PP_Module module_id,
                                     PPB_GetInterface get_browser_interface) {
  DebugPrintf("PPP_InitializeModule: module=%"NACL_PRIu32"\n", module_id);
  SetPPBGetInterface(get_browser_interface, plugin_->enable_dev_interface());
  PPBGetInterfaces();

  SetBrowserPppForInstance(plugin_->pp_instance(), this);
  CHECK(main_channel_ != NULL);
  nacl::scoped_ptr<nacl::DescWrapper> wrapper(
      BrowserUpcall::Start(&upcall_thread_, main_channel_));
  if (wrapper.get() == NULL)
    return PP_ERROR_FAILED;
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
  SetInstanceIdForSrpcChannel(main_channel_, plugin_->pp_instance());
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
  is_nexe_alive_ = (srpc_result != NACL_SRPC_RESULT_INTERNAL);
  if (srpc_result != NACL_SRPC_RESULT_OK)
    return PP_ERROR_FAILED;
  DebugPrintf("PPP_InitializeModule: pp_error=%"NACL_PRId32"\n", pp_error);
  if (pp_error != PP_OK)
    return pp_error;
  const void* ppp_instance = GetPluginInterface(PPP_INSTANCE_INTERFACE);
  DebugPrintf("PPP_InitializeModule: ppp_instance=%p\n", ppp_instance);
  ppp_instance_interface_ = reinterpret_cast<const PPP_Instance*>(ppp_instance);
  if (ppp_instance_interface_ == NULL)  // PPP_Instance is required.
    return PP_ERROR_NOINTERFACE;
  // PPB_Messaging and PPP_InputEvent are optional, so it's OK for them to
  // return NULL.
  ppp_messaging_interface_ = reinterpret_cast<const PPP_Messaging*>(
        GetPluginInterface(PPP_MESSAGING_INTERFACE));
  ppp_input_event_interface_ = reinterpret_cast<const PPP_InputEvent*>(
        GetPluginInterface(PPP_INPUT_EVENT_INTERFACE));
  if (!is_valid())  // Nexe died in PPP_GetInterface.
    return PP_ERROR_FAILED;
  return PP_OK;
}

void BrowserPpp::ShutdownModule() {
  DebugPrintf("PPP_Shutdown: main_channel=%p\n",
              static_cast<void*>(main_channel_));
  if (main_channel_ == NULL) {
    CHECK(!is_nexe_alive_);
    return;  // The proxy has already been shut down.
  }
  NaClSrpcError srpc_result =
    PppRpcClient::PPP_ShutdownModule(main_channel_);
  DebugPrintf("PPP_ShutdownModule: %s\n", NaClSrpcErrorString(srpc_result));
  NaClThreadJoin(&upcall_thread_);
  UnsetBrowserPppForInstance(plugin_->pp_instance());
  UnsetModuleIdForSrpcChannel(main_channel_);
  UnsetInstanceIdForSrpcChannel(main_channel_);
  main_channel_ = NULL;
  is_nexe_alive_ = false;
  DebugPrintf("PPP_Shutdown: done\n");
}

const void* BrowserPpp::GetPluginInterface(const char* interface_name) {
  DebugPrintf("PPP_GetInterface('%s')\n", interface_name);
  if (!is_valid())
    return NULL;
  int32_t exports_interface_name;
  NaClSrpcError srpc_result =
      PppRpcClient::PPP_GetInterface(main_channel_,
                                     const_cast<char*>(interface_name),
                                     &exports_interface_name);
  DebugPrintf("PPP_GetInterface('%s'): %s\n",
              interface_name, NaClSrpcErrorString(srpc_result));
  is_nexe_alive_ = (srpc_result != NACL_SRPC_RESULT_INTERNAL);

  const void* ppp_interface = NULL;
  if (srpc_result != NACL_SRPC_RESULT_OK || !exports_interface_name) {
    ppp_interface = NULL;
  } else if (strcmp(interface_name, PPP_INSTANCE_INTERFACE) == 0) {
    ppp_interface =
       reinterpret_cast<const void*>(BrowserInstance::GetInterface());
  } else if (strcmp(interface_name, PPP_MESSAGING_INTERFACE) == 0) {
    ppp_interface =
        reinterpret_cast<const void*>(BrowserMessaging::GetInterface());
  } else if (strcmp(interface_name, PPP_INPUT_EVENT_INTERFACE) == 0) {
    ppp_interface =
       reinterpret_cast<const void*>(BrowserInputEvent::GetInterface());
  } else if (strcmp(interface_name, PPP_FIND_DEV_INTERFACE) == 0) {
    ppp_interface =
       reinterpret_cast<const void*>(BrowserFind::GetInterface());
  } else if (strcmp(interface_name, PPP_PRINTING_DEV_INTERFACE) == 0) {
    ppp_interface =
       reinterpret_cast<const void*>(BrowserPrinting::GetInterface());
  } else if (strcmp(interface_name, PPP_SCROLLBAR_DEV_INTERFACE) == 0) {
    ppp_interface =
       reinterpret_cast<const void*>(BrowserScrollbar::GetInterface());
  } else if (strcmp(interface_name, PPP_SELECTION_DEV_INTERFACE) == 0) {
    ppp_interface =
       reinterpret_cast<const void*>(BrowserSelection::GetInterface());
  } else if (strcmp(interface_name, PPP_WIDGET_DEV_INTERFACE) == 0) {
    ppp_interface =
       reinterpret_cast<const void*>(BrowserWidget::GetInterface());
  } else if (strcmp(interface_name, PPP_ZOOM_DEV_INTERFACE) == 0) {
    ppp_interface =
       reinterpret_cast<const void*>(BrowserZoom::GetInterface());
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

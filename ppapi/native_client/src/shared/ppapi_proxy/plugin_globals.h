// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_GLOBALS_H_
#define NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_GLOBALS_H_

#include "ppapi/c/dev/ppb_memory_dev.h"
#include "ppapi/c/dev/ppp_find_dev.h"
#include "ppapi/c/dev/ppp_printing_dev.h"
#include "ppapi/c/dev/ppp_scrollbar_dev.h"
#include "ppapi/c/dev/ppp_selection_dev.h"
#include "ppapi/c/dev/ppp_widget_dev.h"
#include "ppapi/c/dev/ppp_zoom_dev.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/ppb.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/c/ppp_input_event.h"
#include "ppapi/c/ppp_instance.h"
#include "ppapi/c/ppp_messaging.h"
#include "native_client/src/untrusted/irt/irt_ppapi.h"

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

// Support for getting PPB_ browser interfaces.
// Safe version CHECK's for NULL.
const void* GetBrowserInterface(const char* interface_name);
const void* GetBrowserInterfaceSafe(const char* interface_name);
// Functions marked "shared" are to be provided by both the browser and the
// plugin side of the proxy, so they can be used by the shared proxy code
// under both trusted and untrusted compilation.
const PPB_Core* PPBCoreInterface();  // shared
const PPB_Memory_Dev* PPBMemoryInterface();  // shared
const PPB_Var* PPBVarInterface();  // shared

// Support for getting PPP_ plugin interfaces.
// Safe version CHECK's for NULL.
// Since no PppRpcServer function will be called if the interface is NULL,
// safe version is used to define interface getters below.
const void* GetPluginInterface(const char* interface_name);
const void* GetPluginInterfaceSafe(const char* interface_name);
const PPP_Find_Dev* PPPFindInterface();
const PPP_InputEvent* PPPInputEventInterface();
const PPP_Instance* PPPInstanceInterface();
const PPP_Messaging* PPPMessagingInterface();
const PPP_Printing_Dev* PPPPrintingInterface();
const PPP_Scrollbar_Dev* PPPScrollbarInterface();
const PPP_Selection_Dev* PPPSelectionInterface();
const PPP_Widget_Dev* PPPWidgetInterface();
const PPP_Zoom_Dev* PPPZoomInterface();

// Get thread creation/join functions.
const struct PP_ThreadFunctions* GetThreadCreator();

// PPAPI constants used in the proxy.
extern const PP_Resource kInvalidResourceId;

}  // namespace ppapi_proxy

#endif  // NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_GLOBALS_H_

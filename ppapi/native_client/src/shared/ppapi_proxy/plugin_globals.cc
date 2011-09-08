// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/nacl_imc_api.h>
#include <sys/nacl_syscalls.h>

#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/plugin_ppb_core.h"
#include "native_client/src/shared/ppapi_proxy/plugin_ppb_var.h"
#include "native_client/src/shared/ppapi_proxy/ppruntime.h"
#include "native_client/src/shared/ppapi_proxy/untrusted/srpcgen/ppp_rpc.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"


#define NACL_SEND_FD 6

namespace {

NaClSrpcChannel* main_srpc_channel;
NaClSrpcChannel* upcall_srpc_channel;
PP_Module module_id_for_plugin;
struct PP_ThreadFunctions thread_funcs;

}  // namespace

namespace ppapi_proxy {

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

const struct PP_ThreadFunctions* GetThreadCreator() {
  return &thread_funcs;
}

// Browser interface helpers

const void* GetBrowserInterfaceSafe(const char* interface_name) {
  const void* ppb_interface = GetBrowserInterface(interface_name);
  if (ppb_interface == NULL)
    DebugPrintf("PPB_GetInterface: %s not found\n", interface_name);
  CHECK(ppb_interface != NULL);
  return ppb_interface;
}

const PPB_Core* PPBCoreInterface() {
  return static_cast<const PPB_Core*>(
      GetBrowserInterfaceSafe(PPB_CORE_INTERFACE));
}

const PPB_Memory_Dev* PPBMemoryInterface() {
  return static_cast<const PPB_Memory_Dev*>(
      GetBrowserInterfaceSafe(PPB_MEMORY_DEV_INTERFACE));
}

const PPB_Var* PPBVarInterface() {
  return static_cast<const PPB_Var*>(
      GetBrowserInterfaceSafe(PPB_VAR_INTERFACE));
}

// Plugin interface helpers

const void* GetPluginInterface(const char* interface_name) {
  return ::PPP_GetInterface(interface_name);
}

const void* GetPluginInterfaceSafe(const char* interface_name) {
  const void* ppp_interface = GetPluginInterface(interface_name);
  if (ppp_interface == NULL)
    DebugPrintf("PPP_GetInterface: %s not found\n", interface_name);
  CHECK(ppp_interface != NULL);
  return ppp_interface;
}

const PPP_Find_Dev* PPPFindInterface() {
  static const void* ppp = GetPluginInterfaceSafe(PPP_FIND_DEV_INTERFACE);
  return static_cast<const PPP_Find_Dev*>(ppp);
}

const PPP_InputEvent* PPPInputEventInterface() {
  static const void* ppp = GetPluginInterfaceSafe(PPP_INPUT_EVENT_INTERFACE);
  return static_cast<const PPP_InputEvent*>(ppp);
}

const PPP_Instance* PPPInstanceInterface() {
  static const void* ppp = GetPluginInterfaceSafe(PPP_INSTANCE_INTERFACE);
  return static_cast<const PPP_Instance*>(ppp);
}

const PPP_Messaging* PPPMessagingInterface() {
  static const void* ppp = GetPluginInterfaceSafe(PPP_MESSAGING_INTERFACE);
  return static_cast<const PPP_Messaging*>(ppp);
}

const PPP_MouseLock_Dev* PPPMouseLockInterface() {
  static const void* ppp = GetPluginInterfaceSafe(PPP_MOUSELOCK_DEV_INTERFACE);
  return static_cast<const PPP_MouseLock_Dev*>(ppp);
}

const PPP_Printing_Dev* PPPPrintingInterface() {
  static const void* ppp = GetPluginInterfaceSafe(PPP_PRINTING_DEV_INTERFACE);
  return static_cast<const PPP_Printing_Dev*>(ppp);
}

const PPP_Scrollbar_Dev* PPPScrollbarInterface() {
  static const void* ppp = GetPluginInterfaceSafe(PPP_SCROLLBAR_DEV_INTERFACE);
  return static_cast<const PPP_Scrollbar_Dev*>(ppp);
}

const PPP_Selection_Dev* PPPSelectionInterface() {
  static const void* ppp = GetPluginInterfaceSafe(PPP_SELECTION_DEV_INTERFACE);
  return static_cast<const PPP_Selection_Dev*>(ppp);
}

const PPP_Widget_Dev* PPPWidgetInterface() {
  static const void* ppp = GetPluginInterfaceSafe(PPP_WIDGET_DEV_INTERFACE);
  return static_cast<const PPP_Widget_Dev*>(ppp);
}

const PPP_Zoom_Dev* PPPZoomInterface() {
  static const void* ppp = GetPluginInterfaceSafe(PPP_ZOOM_DEV_INTERFACE);
  return static_cast<const PPP_Zoom_Dev*>(ppp);
}


}  // namespace ppapi_proxy

void PpapiPluginRegisterThreadCreator(
    const struct PP_ThreadFunctions* new_funcs) {
  thread_funcs = *new_funcs;
}

int IrtInit() {
  // TODO(halyavin): this is needed for tests without IRT. They do not start
  // in irt_entry.c where IrtInit is called.
  static int initialized = 0;
  if (initialized) {
    return 0;
  }
  if (!NaClSrpcModuleInit()) {
    return 1;
  }
  NaClLogModuleInit();  // Enable NaClLog'ing used by CHECK().
  initialized = 1;
  return 0;
}

int PpapiPluginMain() {
  IrtInit();
  PpapiPluginRegisterDefaultThreadCreator();
  // Designate this as the main thread for PPB_Core::IsMainThread().
  ppapi_proxy::PluginCore::MarkMainThread();
  if (!NaClSrpcAcceptClientConnection(PppRpcs::srpc_methods)) {
    return 1;
  }
  NaClSrpcModuleFini();

  return 0;
}

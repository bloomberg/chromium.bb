// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/nacl_imc_api.h>
#include <sys/nacl_syscalls.h>

#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/plugin_ppb_core.h"
#include "native_client/src/shared/ppapi_proxy/plugin_ppb_var_deprecated.h"
#include "native_client/src/shared/ppapi_proxy/ppruntime.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "srpcgen/ppp_rpc.h"


#define NACL_SEND_FD 6

namespace {

NaClSrpcChannel* main_srpc_channel;
NaClSrpcChannel* upcall_srpc_channel;
PP_Module module_id_for_plugin;
struct PP_ThreadFunctions thread_funcs;

}  // namespace;

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

const void* GetBrowserInterfaceSafe(const char* interface_name) {
  const void* ppb_interface = GetBrowserInterface(interface_name);
  if (ppb_interface == NULL)
    DebugPrintf("PPB_GetInterface: %s not found\n", interface_name);
  CHECK(ppb_interface);
  return ppb_interface;
}

const PPB_Core* PPBCoreInterface() {
  return reinterpret_cast<const PPB_Core*>(
      GetBrowserInterfaceSafe(PPB_CORE_INTERFACE));
}

const PPB_Var_Deprecated* PPBVarDeprecatedInterface() {
  return reinterpret_cast<const PPB_Var_Deprecated*>(
      GetBrowserInterfaceSafe(PPB_VAR_DEPRECATED_INTERFACE));
}

const PPP_Messaging* PPPMessagingInterface() {
  static const PPP_Messaging* ppp_messaging = static_cast<const PPP_Messaging*>(
      ::PPP_GetInterface(PPP_MESSAGING_INTERFACE));
  // This helper is only used from interface function proxies; a NULL return
  // means something is wrong with the proxy.
  CHECK(ppp_messaging != NULL);
  return ppp_messaging;
}

const struct PP_ThreadFunctions* GetThreadCreator() {
  return &thread_funcs;
}

}  // namespace ppapi_proxy

void PpapiPluginRegisterThreadCreator(
    const struct PP_ThreadFunctions* new_funcs) {
  thread_funcs = *new_funcs;
}

int PpapiPluginMain() {
  if (getenv("NACL_LD_ACCEPTS_PLUGIN_CONNECTION") != NULL) {
    // Send a message to the page to ask it to reinitialise the
    // plugin's SRPC/PPAPI connection.  This triggers a call to
    // __startSrpcServices() which in turn calls
    // StartProxiedExecution() which sets up the PPAPI proxy.  This is
    // necessary because LoadNaClModule()'s earlier call to
    // StartSrpcServices() was matched by the dynamic linker before
    // libppruntime was available.
    // For background, see:
    // http://code.google.com/p/nativeclient/issues/detail?id=617
    // http://code.google.com/p/nativeclient/issues/detail?id=1501
    // TODO(mseaborn): This is a temporary measure.  Find a less hacky
    // way to do this.
    struct NaClImcMsgIoVec iov = { const_cast<char*>("Init"), 4 };
    struct NaClImcMsgHdr message = { &iov, 1, NULL, 0, 0 };
    imc_sendmsg(NACL_SEND_FD, &message, 0);
  }

  if (!NaClSrpcModuleInit()) {
    return 1;
  }
  NaClLogModuleInit();  // Enable NaClLog'ing used by CHECK().
  PpapiPluginRegisterDefaultThreadCreator();
  // Designate this as the main thread for PPB_Core::IsMainThread().
  ppapi_proxy::PluginCore::MarkMainThread();
  if (!NaClSrpcAcceptClientConnection(PppRpcs::srpc_methods)) {
    return 1;
  }
  NaClSrpcModuleFini();

  return 0;
}

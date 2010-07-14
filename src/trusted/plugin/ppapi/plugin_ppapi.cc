/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/trusted/plugin/ppapi/browser_interface_ppapi.h"
#include "native_client/src/trusted/plugin/ppapi/plugin_ppapi.h"

#include "native_client/src/include/nacl_base.h"
#include "native_client/src/include/nacl_macros.h"

namespace plugin {

PluginPpapi* PluginPpapi::New(PP_Instance pp_instance) {
  PLUGIN_PRINTF(("PluginPpapi::New: pp_instance=%"NACL_PRId64")\n",
                 pp_instance));
#if NACL_WINDOWS && !defined(NACL_STANDALONE)
  if (!NaClHandlePassBrowserCtor()) {
    return NULL;
  }
#endif
  PluginPpapi* plugin = new(std::nothrow) PluginPpapi(pp_instance);
  if (plugin == NULL) {
    return NULL;
  }
  PLUGIN_PRINTF(("PluginPpapi::New: plugin=%p - done\n",
                 static_cast<void*>(plugin)));
  return plugin;
}


bool PluginPpapi::Init(uint32_t argc, const char* argn[], const char* argv[]) {
  PLUGIN_PRINTF(("PluginPpapi::Init: argc=%"NACL_PRIu32"\n", argc));
  BrowserInterface* browser_interface =
      static_cast<BrowserInterface*>(new(std::nothrow) BrowserInterfacePpapi);
  if (browser_interface == NULL) {
    return false;
  }
  ScriptableHandle* handle = browser_interface->NewScriptableHandle(this);
  if (handle == NULL) {
    PLUGIN_PRINTF(("PluginPpapi::Init: NewScriptableHandle()=NULL\n"));
    return false;
  }
  set_scriptable_handle(handle);
  bool status =
      Plugin::Init(browser_interface,
                   reinterpret_cast<InstanceIdentifier>(pp_instance()),
                   static_cast<int>(argc),
                   const_cast<char**>(argn),
                   const_cast<char**>(argv));
  PLUGIN_PRINTF(("PluginPpapi::Init: status=%d - done\n", status));
  return status;
}


PluginPpapi::PluginPpapi(PP_Instance pp_instance)
    : pp::Instance(pp_instance) {
  PLUGIN_PRINTF(("PluginPpapi::PluginPpapi: this=%p, pp_instance=%"
                 NACL_PRId64"\n", static_cast<void* >(this), pp_instance));
}


PluginPpapi::~PluginPpapi() {
  PLUGIN_PRINTF(("PluginPpapi::~PluginPpapi: this=%p\n",
                 static_cast<void* >(this)));
}


bool PluginPpapi::RequestNaClModule(const nacl::string& url) {
  UNREFERENCED_PARAMETER(url);
  return false;
}


void PluginPpapi::StartProxiedExecution(NaClSrpcChannel* srpc_channel) {
  UNREFERENCED_PARAMETER(srpc_channel);
}

}  // namespace plugin

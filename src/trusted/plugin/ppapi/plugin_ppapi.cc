/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <string>

#include "native_client/src/trusted/plugin/ppapi/browser_interface_ppapi.h"
#include "native_client/src/trusted/plugin/ppapi/plugin_ppapi.h"
#include "native_client/src/trusted/plugin/ppapi/scriptable_handle_ppapi.h"
#include "native_client/src/trusted/plugin/scriptable_handle.h"

#include "native_client/src/include/nacl_base.h"
#include "native_client/src/include/nacl_macros.h"

namespace plugin {

PluginPpapi* PluginPpapi::New(PP_Instance pp_instance) {
  PLUGIN_PRINTF(("PluginPpapi::New (pp_instance=%"NACL_PRId64")\n",
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
  PLUGIN_PRINTF(("PluginPpapi::New (return %p)\n",
                 static_cast<void*>(plugin)));
  return plugin;
}


bool PluginPpapi::Init(uint32_t argc, const char* argn[], const char* argv[]) {
  PLUGIN_PRINTF(("PluginPpapi::Init (argc=%"NACL_PRIu32")\n", argc));
  BrowserInterface* browser_interface =
      static_cast<BrowserInterface*>(new(std::nothrow) BrowserInterfacePpapi);
  if (browser_interface == NULL) {
    return false;
  }
  ScriptableHandle* handle = browser_interface->NewScriptableHandle(this);
  if (handle == NULL) {
    PLUGIN_PRINTF(("PluginPpapi::Init (scriptable handle creation failed)\n"));
    return false;
  }
  set_scriptable_handle(handle);
  PLUGIN_PRINTF(("PluginPpapi::Init (scriptable_handle=%p)\n",
                 static_cast<void*>(scriptable_handle())));
  bool status =
      Plugin::Init(browser_interface,
                   PPInstanceToInstanceIdentifier(
                       static_cast<pp::Instance*>(this)),
                   static_cast<int>(argc),
                   const_cast<char**>(argn),
                   const_cast<char**>(argv));
  if (status) {
    for (uint32_t i = 0; i < argc; ++i) {
      if (strcmp(argn[i], "src") == 0)
        status = RequestNaClModule(argv[i]);
    }
  }

  PLUGIN_PRINTF(("PluginPpapi::Init (return %d)\n", status));
  return status;
}


PluginPpapi::PluginPpapi(PP_Instance pp_instance)
    : pp::Instance(pp_instance) {
  PLUGIN_PRINTF(("PluginPpapi::PluginPpapi (this=%p, pp_instance=%"
                 NACL_PRId64")\n", static_cast<void*>(this), pp_instance));
}


PluginPpapi::~PluginPpapi() {
  PLUGIN_PRINTF(("PluginPpapi::~PluginPpapi (this=%p)\n",
                 static_cast<void*>(this)));
}


pp::Var PluginPpapi::GetInstanceObject() {
  ScriptableHandlePpapi* handle =
      static_cast<ScriptableHandlePpapi*>(scriptable_handle());
  PLUGIN_PRINTF(("PluginPpapi::GetInstanceObject "
                 "(this=%p, scriptable_handle=%p)\n",
                 static_cast<void*>(this), static_cast<void*>(handle)));
  return handle;
}


bool PluginPpapi::RequestNaClModule(const nacl::string& url) {
  PLUGIN_PRINTF(("PluginPpapi::RequestNaClModule (url='%s')\n", url.c_str()));
  // TODO(polina): when URLLoader is supported, use that to get the
  // the local copy of the nexe at |url|
  nacl::string full_url = "";
  if (!browser_interface()->GetFullURL(
          PPInstanceToInstanceIdentifier(static_cast<pp::Instance*>(this)),
                                         &full_url)) {
    PLUGIN_PRINTF(("PluginPpapi::RequestNaClModule (unknown page link)\n"));
    return false;
  }
  size_t last_slash = full_url.rfind("/");
  if (last_slash == nacl::string::npos) {
    PLUGIN_PRINTF(("PluginPpapi::RequestNaClModule (search for '/' failed)\n"));
  }
  full_url.replace(last_slash + 1, full_url.size(), url);
  PLUGIN_PRINTF(("PluginPpapi::RequestNaClModule (full_url='%s')\n",
                 full_url.c_str()));
  char* local_origin = getenv("NACL_PPAPI_LOCAL_ORIGIN");
  if (local_origin == NULL) {
    PLUGIN_PRINTF(("PluginPpapi::RequestNaClModule "
                   "($NACL_PPAPI_LOCAL_ORIGIN is not set)\n"));
    return false;
  }
  nacl::string local_path = full_url;
  local_path.replace(0, origin().size(), local_origin);
  PLUGIN_PRINTF(("PluginPpapi::RequestNaClModule (local_path='%s')\n",
                 local_path.c_str()));
  return Load(full_url, local_path.c_str());
}


void PluginPpapi::StartProxiedExecution(NaClSrpcChannel* srpc_channel) {
  UNREFERENCED_PARAMETER(srpc_channel);
}

}  // namespace plugin

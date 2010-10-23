/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/trusted/plugin/ppapi/plugin_ppapi.h"

#include <fcntl.h>
#include <string>

#include "native_client/src/include/portability_io.h"
#include "native_client/src/shared/ppapi_proxy/browser_ppp.h"
#include "native_client/src/trusted/plugin/ppapi/browser_interface_ppapi.h"
#include "native_client/src/trusted/plugin/ppapi/scriptable_handle_ppapi.h"
#include "native_client/src/trusted/plugin/scriptable_handle.h"

#include "native_client/src/include/nacl_base.h"
#include "native_client/src/include/nacl_macros.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppp_instance.h"
#include "ppapi/cpp/module.h"

namespace {
const char* kSrcAttribute = "src";  // The "src" attr of the <embed> tag.
const char* kNexesAttribute = "nexes";  // The "nexes" attr of the <embed> tag.
}  // namespace

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
    return false;
  }
  set_scriptable_handle(handle);
  PLUGIN_PRINTF(("PluginPpapi::Init (scriptable_handle=%p)\n",
                 static_cast<void*>(scriptable_handle())));
  bool status = Plugin::Init(browser_interface,
                             PPInstanceToInstanceIdentifier(
                                 static_cast<pp::Instance*>(this)),
                             static_cast<int>(argc),
                             const_cast<char**>(argn),
                             const_cast<char**>(argv));
  if (status) {
    const char* src_attr = LookupArgument(kSrcAttribute);
    PLUGIN_PRINTF(("PluginPpapi::Init (src_attr=%s)\n", src_attr));
    if (src_attr != NULL) {
      status = RequestNaClModule(src_attr);
    } else {
      // If there was no "src" attribute, then look for a "nexes" attribute
      // and try to load the ISA defined for this particular sandbox.
      const char* nexes_attr = LookupArgument(kNexesAttribute);
      PLUGIN_PRINTF(("PluginPpapi::Init (nexes_attr=%s)\n", nexes_attr));
      if (nexes_attr != NULL) {
        status = SetNexesPropertyImpl(nexes_attr);
      }
    }
  }

  PLUGIN_PRINTF(("PluginPpapi::Init (return %d)\n", status));
  return status;
}


PluginPpapi::PluginPpapi(PP_Instance pp_instance)
    : pp::Instance(pp_instance), ppapi_proxy_(NULL) {
  PLUGIN_PRINTF(("PluginPpapi::PluginPpapi (this=%p, pp_instance=%"
                 NACL_PRId64")\n", static_cast<void*>(this), pp_instance));
}


PluginPpapi::~PluginPpapi() {
  PLUGIN_PRINTF(("PluginPpapi::~PluginPpapi (this=%p, scriptable_handle=%p)\n",
                 static_cast<void*>(this),
                 static_cast<void*>(scriptable_handle())));

#if NACL_WINDOWS && !defined(NACL_STANDALONE)
  NaClHandlePassBrowserDtor();
#endif

  delete ppapi_proxy_;
  ScriptableHandle* scriptable_handle_ = scriptable_handle();
  UnrefScriptableHandle(&scriptable_handle_);
}


pp::Var PluginPpapi::GetInstanceObject() {
  PLUGIN_PRINTF(("PluginPpapi::GetInstanceObject (this=%p)\n",
                 static_cast<void*>(this)));
  ScriptableHandlePpapi* handle =
      static_cast<ScriptableHandlePpapi*>(scriptable_handle()->AddRef());
  if (ppapi_proxy_ == NULL) {
    pp::Var* handle_var = handle->var();
    PLUGIN_PRINTF(("PluginPpapi::GetInstanceObject (handle=%p, "
                   "handle_var=%p)\n",
                  static_cast<void*>(handle), static_cast<void*>(handle_var)));
    return *handle_var;  // make a copy
  } else {
    // TODO(sehr): cache the instance_interface on the plugin.
    const PPP_Instance* instance_interface =
        reinterpret_cast<const PPP_Instance*>(
            ppapi_proxy_->GetInterface(PPP_INSTANCE_INTERFACE));
    if (instance_interface == NULL) {
      pp::Var* handle_var = handle->var();
      // TODO(sehr): report an error here.
      return *handle_var;  // make a copy
    }
    // Yuck.  This feels like another low-level interface usage.
    // TODO(sehr,polina): add a better interface to rebuild Vars from
    // low-level components we proxy.
    return pp::Var(pp::Var::PassRef(),
                   instance_interface->GetInstanceObject(pp_instance()));
  }
}


bool PluginPpapi::RequestNaClModule(const nacl::string& url) {
  PLUGIN_PRINTF(("PluginPpapi::RequestNaClModule (url='%s')\n", url.c_str()));
  // TODO(polina): when URLLoader is supported, get the file descriptor from it.
  // For now, to simulate the planned behavior, we just get the full url, map it
  // to a local path, then use that to open a file and get the file descriptor.
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
  int file_desc = OPEN(local_path.c_str(), O_RDONLY);
  PLUGIN_PRINTF(("PluginPpapi::RequestNaClModule (file_desc=%d)\n", file_desc));
  if (file_desc > NACL_NO_FILE_DESC) {
    return LoadNaClModule(full_url, file_desc);
  } else {
    return false;
  }
}


void PluginPpapi::StartProxiedExecution(NaClSrpcChannel* srpc_channel) {
  PLUGIN_PRINTF(("PluginPpapi::StartProxiedExecution (%p)\n",
                 reinterpret_cast<void*>(srpc_channel)));
  // Check that the .nexe exports the PPAPI intialization method.
  NaClSrpcService* client_service = srpc_channel->client;
  if (NaClSrpcServiceMethodIndex(client_service,
                                 "PPP_InitializeModule:ilhs:ii") ==
      kNaClSrpcInvalidMethodIndex) {
    return;
  }
  ppapi_proxy_ =
      new(std::nothrow) ppapi_proxy::BrowserPpp(srpc_channel);
  PLUGIN_PRINTF(("PluginPpapi::StartProxiedExecution (ppapi_proxy_ = %p)\n",
                 reinterpret_cast<void*>(ppapi_proxy_)));
  if (ppapi_proxy_ == NULL) {
    return;
  }
  pp::Module* module = pp::Module::Get();
  PLUGIN_PRINTF(("PluginPpapi::StartProxiedExecution (module = %p)\n",
                 reinterpret_cast<void*>(module)));
  if (module == NULL) {
    return;
  }
  int32_t init_retval =
      ppapi_proxy_->InitializeModule(module->pp_module(),
                                     module->get_browser_interface(),
                                     pp_instance());
  if (init_retval != PP_OK) {
    // TODO(sehr): we should report an error here and shut down the proxy.
    // For now we will leak the proxy, but no longer be allowed to access it.
    ppapi_proxy_ = NULL;
    return;
  }
  const PPP_Instance* instance_interface =
      reinterpret_cast<const PPP_Instance*>(
          ppapi_proxy_->GetInterface(PPP_INSTANCE_INTERFACE));
  if (instance_interface == NULL) {
    // TODO(sehr): we should report an error here and shut down the proxy.
    // For now we will leak the proxy, but no longer be allowed to access it.
    ppapi_proxy_ = NULL;
    return;
  }
  // Create an instance and initialize the instance's parameters.
  if (!instance_interface->DidCreate(pp_instance(),
                                     argc(),
                                     const_cast<const char**>(argn()),
                                     const_cast<const char**>(argv()))) {
    // TODO(sehr): we should report an error here.
    return;
  }
}

}  // namespace plugin

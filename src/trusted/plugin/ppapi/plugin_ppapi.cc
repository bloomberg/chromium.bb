/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/trusted/plugin/ppapi/plugin_ppapi.h"

#include "native_client/src/include/nacl_base.h"
#include "native_client/src/include/nacl_macros.h"

namespace plugin {

PluginPpapi* PluginPpapi::New(PP_Instance instance) {
  PLUGIN_PRINTF(("PluginPpapi::New(%"NACL_PRId64")\n", instance));
  NACL_UNIMPLEMENTED();

  PluginPpapi* plugin = new(std::nothrow) PluginPpapi(instance);
  if (plugin == NULL) {
    return NULL;
  }
  PLUGIN_PRINTF(("PluginPpapi::New(%p): done\n", static_cast<void*>(plugin)));
  return plugin;
}


PluginPpapi::PluginPpapi(PP_Instance instance)
    : pp::Instance(instance) {
  PLUGIN_PRINTF(("PluginPpapi::~PluginPpapi(%p, %"NACL_PRId64")\n",
                 static_cast<void* >(this), instance));
}


PluginPpapi::~PluginPpapi() {
  PLUGIN_PRINTF(("PluginPpapi::~PluginPpapi(%p)\n", static_cast<void* >(this)));
}


bool PluginPpapi::RequestNaClModule(const nacl::string& url) {
  UNREFERENCED_PARAMETER(url);
  return false;
}


void PluginPpapi::StartProxiedExecution(NaClSrpcChannel* srpc_channel) {
  UNREFERENCED_PARAMETER(srpc_channel);
}

}  // namespace plugin

/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/trusted/desc/nrd_all_modules.h"
#include "native_client/src/trusted/plugin/ppapi/plugin_ppapi.h"
#include "native_client/src/trusted/service_runtime/expiration.h"

#include "ppapi/cpp/module.h"

namespace plugin {

class ModulePpapi : public pp::Module {
 public:
  // TODO(polina): factor out the code below as it is identical to
  // NPP_Initialize and NPP_Shutdown
  ModulePpapi() : pp::Module() {
    PLUGIN_PRINTF(("ModulePpapi::ModulePpapi: %p\n", static_cast<void*>(this)));
    if (NaClHasExpired()) {
      return;
    }
    NaClNrdAllModulesInit();
  }

  virtual ~ModulePpapi() {
    NaClNrdAllModulesFini();
    PLUGIN_PRINTF(("ModulePpapi::~ModulePpapi: %p\n",
                   static_cast<void*>(this)));
  }

  virtual pp::Instance* CreateInstance(PP_Instance pp_instance) {
    PLUGIN_PRINTF(("ModulePpapi::CreateInstance: pp_instance=%"NACL_PRId64"\n",
                   pp_instance));
    PluginPpapi* plugin = PluginPpapi::New(pp_instance);
    PLUGIN_PRINTF(("ModulePpapi::CreateInstance: plugin=%p - done\n",
                   static_cast<void* >(plugin)));
    return plugin;
  }
};

}  // namespace plugin


namespace pp {

Module* CreateModule() {
  PLUGIN_PRINTF(("CreateModule\n"));
  return new plugin::ModulePpapi();
}

}  // namespace pp

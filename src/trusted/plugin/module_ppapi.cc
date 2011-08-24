/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/shared/platform/nacl_time.h"
#include "native_client/src/trusted/desc/nrd_all_modules.h"
#include "native_client/src/trusted/handle_pass/browser_handle.h"
#include "native_client/src/trusted/plugin/nacl_entry_points.h"
#include "native_client/src/trusted/plugin/plugin.h"

#include "ppapi/c/private/ppb_nacl_private.h"
#include "ppapi/cpp/module.h"

GetURandomFDFunc get_urandom_fd;

namespace plugin {

class ModulePpapi : public pp::Module {
 public:
  ModulePpapi() : pp::Module(), init_was_successful_(false) {
    PLUGIN_PRINTF(("ModulePpapi::ModulePpapi (this=%p)\n",
                   static_cast<void*>(this)));
  }

  virtual ~ModulePpapi() {
    if (init_was_successful_) {
      NaClNrdAllModulesFini();
    }
    PLUGIN_PRINTF(("ModulePpapi::~ModulePpapi (this=%p)\n",
                   static_cast<void*>(this)));
  }

  virtual bool Init() {
    // Ask the browser for an interface which provides missing functions
    const PPB_NaCl_Private* ptr = reinterpret_cast<const PPB_NaCl_Private*>(
        GetBrowserInterface(PPB_NACL_PRIVATE_INTERFACE));

    if (NULL == ptr) {
      PLUGIN_PRINTF(("ModulePpapi::Init failed: "
                     "GetBrowserInterface returned NULL\n"));
      return false;
    }

    launch_nacl_process = reinterpret_cast<LaunchNaClProcessFunc>(
        ptr->LaunchSelLdr);
    get_urandom_fd = ptr->UrandomFD;

    // In the plugin, we don't need high resolution time of day.
    NaClAllowLowResolutionTimeOfDay();
    NaClNrdAllModulesInit();

#if NACL_WINDOWS && !defined(NACL_STANDALONE)
    NaClHandlePassBrowserInit();
#endif
    init_was_successful_ = true;
    return true;
  }

  virtual pp::Instance* CreateInstance(PP_Instance pp_instance) {
    PLUGIN_PRINTF(("ModulePpapi::CreateInstance (pp_instance=%"NACL_PRId32")\n",
                   pp_instance));
    Plugin* plugin = Plugin::New(pp_instance);
    PLUGIN_PRINTF(("ModulePpapi::CreateInstance (return %p)\n",
                   static_cast<void* >(plugin)));
    return plugin;
  }

 private:
  bool init_was_successful_;
};

}  // namespace plugin


namespace pp {

Module* CreateModule() {
  PLUGIN_PRINTF(("CreateModule ()\n"));
  return new plugin::ModulePpapi();
}

}  // namespace pp

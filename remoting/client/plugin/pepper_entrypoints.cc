// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/pepper_entrypoints.h"

#include "base/message_loop/message_loop.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/ppb_instance.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/private/internal_module.h"
#include "remoting/client/plugin/chromoting_instance.h"

namespace remoting {

class ChromotingModule : public pp::Module {
 protected:
  virtual ChromotingInstance* CreateInstance(PP_Instance instance) OVERRIDE {
    return new ChromotingInstance(instance);
  }
};

// Implementation of Global PPP functions ---------------------------------
int32_t PPP_InitializeModule(PP_Module module_id,
                             PPB_GetInterface get_browser_interface) {
  ChromotingModule* module = new ChromotingModule();
  if (!module->InternalInit(module_id, get_browser_interface)) {
    delete module;
    return PP_ERROR_FAILED;
  }

#if !defined(NDEBUG)
  // Register a global log handler, but only in Debug builds.
  ChromotingInstance::RegisterLogMessageHandler();
#endif

  pp::InternalSetModuleSingleton(module);
  return PP_OK;
}

void PPP_ShutdownModule() {
  delete pp::Module::Get();
  pp::InternalSetModuleSingleton(NULL);
}

const void* PPP_GetInterface(const char* interface_name) {
  if (!pp::Module::Get())
    return NULL;
  return pp::Module::Get()->GetPluginInterface(interface_name);
}

const void* PPP_GetBrowserInterface(const char* interface_name) {
  if (!pp::Module::Get())
    return NULL;
  return pp::Module::Get()->GetBrowserInterface(interface_name);
}

}  // namespace remoting

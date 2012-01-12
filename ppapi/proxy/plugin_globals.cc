// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/plugin_globals.h"

#include "ppapi/proxy/plugin_dispatcher.h"

namespace ppapi {
namespace proxy {

PluginGlobals* PluginGlobals::plugin_globals_ = NULL;

PluginGlobals::PluginGlobals()
    : ppapi::PpapiGlobals(),
      plugin_proxy_delegate_(NULL),
      callback_tracker_(new CallbackTracker) {
  DCHECK(!plugin_globals_);
  plugin_globals_ = this;
}

PluginGlobals::PluginGlobals(ForTest for_test)
    : ppapi::PpapiGlobals(for_test),
      plugin_proxy_delegate_(NULL),
      callback_tracker_(new CallbackTracker) {
  DCHECK(!plugin_globals_);
}

PluginGlobals::~PluginGlobals() {
  DCHECK(plugin_globals_ == this || !plugin_globals_);
  plugin_globals_ = NULL;
}

ResourceTracker* PluginGlobals::GetResourceTracker() {
  return &plugin_resource_tracker_;
}

VarTracker* PluginGlobals::GetVarTracker() {
  return &plugin_var_tracker_;
}

CallbackTracker* PluginGlobals::GetCallbackTrackerForInstance(
    PP_Instance instance) {
  // In the plugin process, the callback tracker is always the same, regardless
  // of the instance.
  return callback_tracker_.get();
}

FunctionGroupBase* PluginGlobals::GetFunctionAPI(PP_Instance inst, ApiID id) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(inst);
  if (dispatcher)
    return dispatcher->GetFunctionAPI(id);
  return NULL;
}

PP_Module PluginGlobals::GetModuleForInstance(PP_Instance instance) {
  // Currently proxied plugins don't use the PP_Module for anything useful.
  return 0;
}

base::Lock* PluginGlobals::GetProxyLock() {
#ifdef ENABLE_PEPPER_THREADING
  return &proxy_lock_;
#else
  return NULL;
#endif
}

bool PluginGlobals::IsPluginGlobals() const {
  return true;
}

}  // namespace proxy
}  // namespace ppapi

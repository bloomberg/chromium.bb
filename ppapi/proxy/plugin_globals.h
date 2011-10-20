// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PLUGIN_GLOBALS_H_
#define PPAPI_PROXY_PLUGIN_GLOBALS_H_

#include "base/compiler_specific.h"
#include "ppapi/proxy/plugin_resource_tracker.h"
#include "ppapi/proxy/plugin_var_tracker.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/shared_impl/ppapi_globals.h"

namespace ppapi {
namespace proxy {

class PPAPI_PROXY_EXPORT PluginGlobals : public PpapiGlobals {
 public:
  PluginGlobals();
  virtual ~PluginGlobals();

  // Getter for the global singleton. Generally, you should use
  // PpapiGlobals::Get() when possible. Use this only when you need some
  // plugin-specific functionality.
  inline static PluginGlobals* Get() { return plugin_globals_; }

  // PpapiGlobals implementation.
  virtual ResourceTracker* GetResourceTracker() OVERRIDE;
  virtual VarTracker* GetVarTracker() OVERRIDE;

  // Getters for the plugin-specific versions.
  PluginResourceTracker* plugin_resource_tracker() {
    return &plugin_resource_tracker_;
  }
  PluginVarTracker* plugin_var_tracker() {
    return &plugin_var_tracker_;
  }

 private:
  static PluginGlobals* plugin_globals_;

  PluginResourceTracker plugin_resource_tracker_;
  PluginVarTracker plugin_var_tracker_;

  DISALLOW_COPY_AND_ASSIGN(PluginGlobals);
};

}  // namespace proxy
}  // namespace ppapi

#endif   // PPAPI_PROXY_PLUGIN_GLOBALS_H_

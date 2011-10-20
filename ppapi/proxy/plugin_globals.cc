// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/plugin_globals.h"

namespace ppapi {
namespace proxy {

PluginGlobals* PluginGlobals::plugin_globals_ = NULL;

PluginGlobals::PluginGlobals() : ppapi::PpapiGlobals() {
  DCHECK(!plugin_globals_);
  plugin_globals_ = this;
}

PluginGlobals::~PluginGlobals() {
  DCHECK(plugin_globals_ == this);
  plugin_globals_ = NULL;
}

ResourceTracker* PluginGlobals::GetResourceTracker() {
  return &plugin_resource_tracker_;
}

VarTracker* PluginGlobals::GetVarTracker() {
  return &plugin_var_tracker_;
}

}  // namespace proxy
}  // namespace ppapi

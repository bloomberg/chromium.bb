// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_proxy_impl.h"

#include "ppapi/c/private/ppb_proxy_private.h"
#include "webkit/plugins/ppapi/plugin_module.h"
#include "webkit/plugins/ppapi/resource_tracker.h"

namespace webkit {
namespace ppapi {

namespace {

void PluginCrashed(PP_Module module) {
  PluginModule* plugin_module = ResourceTracker::Get()->GetModule(module);
  if (plugin_module)
    plugin_module->PluginCrashed();
}

const PPB_Proxy_Private ppb_proxy = {
  &PluginCrashed
};

}  // namespace

// static
const PPB_Proxy_Private* PPB_Proxy_Impl::GetInterface() {
  return &ppb_proxy;
}

}  // namespace ppapi
}  // namespace webkit

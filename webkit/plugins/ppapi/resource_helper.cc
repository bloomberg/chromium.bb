// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/resource_helper.h"

#include "base/logging.h"
#include "ppapi/shared_impl/resource.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/plugin_module.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/resource_tracker.h"

namespace webkit {
namespace ppapi {

// static
PluginInstance* ResourceHelper::GetPluginInstance(
    const ::ppapi::Resource* resource) {
  ResourceTracker* tracker = ResourceTracker::Get();
  return tracker->GetInstance(resource->pp_instance());
}

PluginModule* ResourceHelper::GetPluginModule(
    const ::ppapi::Resource* resource) {
  PluginInstance* instance = GetPluginInstance(resource);
  return instance ? instance->module() : NULL;
}

PluginDelegate* ResourceHelper::GetPluginDelegate(
    const ::ppapi::Resource* resource) {
  PluginInstance* instance = GetPluginInstance(resource);
  return instance ? instance->delegate() : NULL;
}

}  // namespace ppapi
}  // namespace webkit


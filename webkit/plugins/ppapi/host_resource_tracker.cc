// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/host_resource_tracker.h"

#include "ppapi/shared_impl/resource.h"
#include "webkit/plugins/ppapi/callbacks.h"
#include "webkit/plugins/ppapi/plugin_module.h"
#include "webkit/plugins/ppapi/resource_helper.h"

namespace webkit {
namespace ppapi {

HostResourceTracker::HostResourceTracker() {
}

HostResourceTracker::~HostResourceTracker() {
}

void HostResourceTracker::LastPluginRefWasDeleted(::ppapi::Resource* object) {
  ::ppapi::ResourceTracker::LastPluginRefWasDeleted(object);

  // TODO(brettw) this should be removed when we have the callback tracker
  // moved to the shared_impl. This will allow the logic to post aborts for
  // any callbacks directly in the Resource::LastPluginRefWasDeleted function
  // and we can remove this function altogether.
  PluginModule* plugin_module = ResourceHelper::GetPluginModule(object);
  if (plugin_module) {
    plugin_module->GetCallbackTracker()->PostAbortForResource(
        object->pp_resource());
  }
}

}  // namespace ppapi
}  // namespace webkit

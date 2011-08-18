// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/resource.h"

#include "base/logging.h"
#include "webkit/plugins/ppapi/callbacks.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/plugin_module.h"
#include "webkit/plugins/ppapi/resource_tracker.h"

namespace webkit {
namespace ppapi {

Resource::Resource(PluginInstance* instance)
    : ::ppapi::Resource(instance->pp_instance()),
      instance_(instance) {
}

Resource::~Resource() {
}

PP_Resource Resource::GetReference() {
  ResourceTracker* tracker = ResourceTracker::Get();
  tracker->AddRefResource(pp_resource());
  return pp_resource();
}

void Resource::LastPluginRefWasDeleted() {
  instance()->module()->GetCallbackTracker()->PostAbortForResource(
      pp_resource());
}

void Resource::InstanceWasDeleted() {
  ::ppapi::Resource::InstanceWasDeleted();
  instance_ = NULL;
}

}  // namespace ppapi
}  // namespace webkit


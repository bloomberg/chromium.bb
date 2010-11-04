// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/pepper_resource.h"

#include "base/logging.h"
#include "webkit/glue/plugins/pepper_resource_tracker.h"

namespace pepper {

Resource::Resource(PluginModule* module)
    : resource_id_(0), module_(module) {
}

Resource::~Resource() {
}

PP_Resource Resource::GetReference() {
  ResourceTracker *tracker = ResourceTracker::Get();
  if (resource_id_)
    tracker->AddRefResource(resource_id_);
  else
    resource_id_ = tracker->AddResource(this);
  return resource_id_;
}

PP_Resource Resource::GetReferenceNoAddRef() const {
  return resource_id_;
}

void Resource::StoppedTracking() {
  DCHECK(resource_id_ != 0);
  resource_id_ = 0;
}

}  // namespace pepper

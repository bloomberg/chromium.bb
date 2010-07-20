// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/pepper_resource.h"

#include "webkit/glue/plugins/pepper_resource_tracker.h"

namespace pepper {

PP_Resource Resource::GetReference() {
  ResourceTracker *tracker = ResourceTracker::Get();
  if (resource_id_)
    tracker->AddRefResource(resource_id_);
  else
    resource_id_ = tracker->AddResource(this);
  return resource_id_;
}
}  // namespace pepper

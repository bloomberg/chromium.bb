// Copyright (c) 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/plugin_resource.h"

namespace ppapi_proxy {

PluginResource::PluginResource(PluginModule* module)
    : resource_id_(0), module_(module) {
}

PluginResource::~PluginResource() {
}

PP_Resource PluginResource::GetReference() {
  PluginResourceTracker *tracker = PluginResourceTracker::Get();
  if (resource_id_)
    tracker->AddRefResource(resource_id_);
  else
    resource_id_ = tracker->AddResource(this);
  return resource_id_;
}

void PluginResource::StoppedTracking() {
  resource_id_ = 0;
}

}  // namespace ppapi_proxy


// Copyright (c) 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/plugin_resource.h"

namespace ppapi_proxy {

PluginResource::PluginResource()
    : resource_id_(0) {
}

PluginResource::~PluginResource() {
}

PP_Resource PluginResource::GetReference() {
  PluginResourceTracker::Get()->AddRefResource(resource_id_);
  return resource_id_;
}

void PluginResource::StoppedTracking() {
  resource_id_ = 0;
}

}  // namespace ppapi_proxy


// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/pepper_resource.h"

#include "third_party/ppapi/c/pp_resource.h"
#include "webkit/glue/plugins/pepper_resource_tracker.h"

namespace pepper {

Resource::Resource(PluginModule* module) : module_(module) {
  ResourceTracker::Get()->AddResource(this);
}

Resource::~Resource() {
  ResourceTracker::Get()->DeleteResource(this);
}

PP_Resource Resource::GetResource() const {
  return reinterpret_cast<intptr_t>(this);
}

}  // namespace pepper

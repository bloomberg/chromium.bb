// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "o3d/gpu_plugin/np_utils/dynamic_np_object.h"

namespace gpu_plugin {

DynamicNPObject::DynamicNPObject(NPP npp) {
}

void DynamicNPObject::Invalidate() {
  for (PropertyMap::iterator it = properties_.begin();
       it != properties_.end();
       ++it) {
    it->second.Invalidate();
  }
}

bool DynamicNPObject::HasProperty(NPIdentifier name) {
  PropertyMap::iterator it = properties_.find(name);
  return it != properties_.end();
}

bool DynamicNPObject::GetProperty(NPIdentifier name, NPVariant* result) {
  PropertyMap::iterator it = properties_.find(name);
  if (it == properties_.end())
    return false;

  it->second.CopyTo(result);
  return true;
}

bool DynamicNPObject::SetProperty(NPIdentifier name, const NPVariant* value) {
  properties_[name] = *value;
  return true;
}

bool DynamicNPObject::RemoveProperty(NPIdentifier name) {
  properties_.erase(name);
  return false;
}

bool DynamicNPObject::Enumerate(NPIdentifier** names, uint32_t* count) {
  *names = static_cast<NPIdentifier*>(
      NPBrowser::get()->MemAlloc(properties_.size() * sizeof(*names)));
  *count = properties_.size();

  int i = 0;
  for (PropertyMap::iterator it = properties_.begin();
       it != properties_.end();
       ++it) {
    (*names)[i] = it->first;
    ++i;
  }

  return true;
}
}  // namespace gpu_plugin

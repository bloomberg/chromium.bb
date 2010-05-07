// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/pepper_resource_tracker.h"

#include <set>

#include "base/logging.h"
#include "third_party/ppapi/c/pp_resource.h"
#include "webkit/glue/plugins/pepper_resource.h"

namespace pepper {

ResourceTracker::ResourceTracker() {
}

ResourceTracker::~ResourceTracker() {
}

// static
ResourceTracker* ResourceTracker::Get() {
  return Singleton<ResourceTracker>::get();
}

Resource* ResourceTracker::GetResource(PP_Resource res) const {
  AutoLock lock(lock_);
  Resource* resource = reinterpret_cast<Resource*>(res.id);
  if (live_resources_.find(resource) == live_resources_.end())
    return NULL;
  return resource;
}

void ResourceTracker::AddResource(Resource* resource) {
  AutoLock lock(lock_);
  DCHECK(live_resources_.find(resource) == live_resources_.end());
  live_resources_.insert(resource);
}

void ResourceTracker::DeleteResource(Resource* resource) {
  AutoLock lock(lock_);
  ResourceSet::iterator found = live_resources_.find(resource);
  if (found == live_resources_.end()) {
    NOTREACHED();
    return;
  }
  live_resources_.erase(found);
}

}  // namespace pepper

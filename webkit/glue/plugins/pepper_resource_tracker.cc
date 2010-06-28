// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/pepper_resource_tracker.h"

#include <set>

#include "base/logging.h"
#include "third_party/ppapi/c/pp_resource.h"
#include "webkit/glue/plugins/pepper_buffer.h"
#include "webkit/glue/plugins/pepper_device_context_2d.h"
#include "webkit/glue/plugins/pepper_directory_reader.h"
#include "webkit/glue/plugins/pepper_file_chooser.h"
#include "webkit/glue/plugins/pepper_file_io.h"
#include "webkit/glue/plugins/pepper_file_ref.h"
#include "webkit/glue/plugins/pepper_image_data.h"
#include "webkit/glue/plugins/pepper_resource.h"
#include "webkit/glue/plugins/pepper_url_loader.h"
#include "webkit/glue/plugins/pepper_url_request_info.h"
#include "webkit/glue/plugins/pepper_url_response_info.h"

namespace pepper {

ResourceTracker::ResourceTracker() {
}

ResourceTracker::~ResourceTracker() {
}

// static
ResourceTracker* ResourceTracker::Get() {
  return Singleton<ResourceTracker>::get();
}

scoped_refptr<Resource> ResourceTracker::GetResource(PP_Resource res) const {
  AutoLock lock(lock_);
  Resource* resource = reinterpret_cast<Resource*>(res);
  if (live_resources_.find(resource) == live_resources_.end())
    return scoped_refptr<Resource>();
  return scoped_refptr<Resource>(resource);
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

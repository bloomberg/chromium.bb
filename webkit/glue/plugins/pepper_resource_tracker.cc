// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/pepper_resource_tracker.h"

#include <limits>
#include <set>

#include "base/logging.h"
#include "third_party/ppapi/c/pp_resource.h"
#include "webkit/glue/plugins/pepper_resource.h"

namespace pepper {

scoped_refptr<Resource> ResourceTracker::GetResource(PP_Resource res) const {
  ResourceMap::const_iterator result = live_resources_.find(res);
  if (result == live_resources_.end()) {
    return scoped_refptr<Resource>();
  }
  return result->second.first;
}

PP_Resource ResourceTracker::AddResource(Resource* resource) {
  // If the plugin manages to create 4B resources...
  if (last_id_ == std::numeric_limits<PP_Resource>::max()) {
    return 0;
  }
  // Add the resource with plugin use-count 1.
  ++last_id_;
  live_resources_.insert(std::make_pair(last_id_, std::make_pair(resource, 1)));
  return last_id_;
}

bool ResourceTracker::AddRefResource(PP_Resource res) {
  ResourceMap::iterator i = live_resources_.find(res);
  if (i != live_resources_.end()) {
    // We don't protect against overflow, since a plugin as malicious as to ref
    // once per every byte in the address space could have just as well unrefed
    // one time too many.
    ++i->second.second;
    return true;
  } else {
    return false;
  }
}

bool ResourceTracker::UnrefResource(PP_Resource res) {
  ResourceMap::iterator i = live_resources_.find(res);
  if (i != live_resources_.end()) {
    if (!--i->second.second) {
      i->second.first->StoppedTracking();
      live_resources_.erase(i);
    }
    return true;
  } else {
    return false;
  }
}

}  // namespace pepper

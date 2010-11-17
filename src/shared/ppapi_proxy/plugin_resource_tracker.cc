// Copyright (c) 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/plugin_resource_tracker.h"

#include <limits>
#include <set>

#include "native_client/src/shared/ppapi_proxy/plugin_resource.h"
#include "ppapi/c/pp_resource.h"

namespace ppapi_proxy {

scoped_refptr<PluginResource>
PluginResourceTracker::GetResource(PP_Resource res) const {
  ResourceMap::const_iterator result = live_resources_.find(res);
  if (result == live_resources_.end()) {
    return scoped_refptr<PluginResource>();
  }
  return result->second.first;
}

PluginResourceTracker::PluginResourceTracker()
    : last_id_(0) {
}

PluginResourceTracker::~PluginResourceTracker() {
}

PP_Resource PluginResourceTracker::AddResource(PluginResource* resource) {
  // If the plugin manages to create 4B resources...
  if (last_id_ == std::numeric_limits<PP_Resource>::max()) {
    return 0;
  }
  // Add the resource with plugin use-count 1.
  ++last_id_;
  live_resources_.insert(std::make_pair(last_id_, std::make_pair(resource, 1)));
  return last_id_;
}

bool PluginResourceTracker::AddRefResource(PP_Resource res) {
  ResourceMap::iterator i = live_resources_.find(res);
  if (i == live_resources_.end()) {
    return false;
  } else {
    // We don't protect against overflow, since a plugin as malicious as to ref
    // once per every byte in the address space could have just as well unrefed
    // one time too many.
    ++i->second.second;
    return true;
  }
}

bool PluginResourceTracker::UnrefResource(PP_Resource res) {
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

}  // namespace ppapi_proxy


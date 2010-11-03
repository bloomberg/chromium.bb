// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/plugin_resource_tracker.h"

#include "base/logging.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/serialized_var.h"

namespace pp {
namespace proxy {

PluginResourceTracker::ResourceInfo::ResourceInfo() : ref_count(0) {
}

PluginResourceTracker::ResourceInfo::ResourceInfo(int rc,
                                                  linked_ptr<PluginResource> r)
    : ref_count(rc),
      resource(r) {
}

PluginResourceTracker::ResourceInfo::ResourceInfo(const ResourceInfo& other)
    : ref_count(other.ref_count),
      resource(other.resource) {
}

PluginResourceTracker::ResourceInfo::~ResourceInfo() {
}

PluginResourceTracker::ResourceInfo&
PluginResourceTracker::ResourceInfo::operator=(
    const ResourceInfo& other) {
  ref_count = other.ref_count;
  resource = other.resource;
  return *this;
}

PluginResourceTracker::PluginResourceTracker(PluginDispatcher* dispatcher)
    : dispatcher_(dispatcher) {
}

PluginResourceTracker::~PluginResourceTracker() {
}

PluginResource* PluginResourceTracker::GetResourceObject(
    PP_Resource pp_resource) {
  ResourceMap::iterator found = resource_map_.find(pp_resource);
  if (found == resource_map_.end())
    return NULL;
  return found->second.resource.get();
}

void PluginResourceTracker::AddResource(PP_Resource pp_resource,
                                        linked_ptr<PluginResource> object) {
  DCHECK(resource_map_.find(pp_resource) == resource_map_.end());
  resource_map_[pp_resource] = ResourceInfo(1, object);
}

void PluginResourceTracker::AddRefResource(PP_Resource resource) {
  resource_map_[resource].ref_count++;
}

void PluginResourceTracker::ReleaseResource(PP_Resource resource) {
  ReleasePluginResourceRef(resource, true);
}

void PluginResourceTracker::ReleasePluginResourceRef(
    const PP_Resource& resource,
    bool notify_browser_on_release) {
  ResourceMap::iterator found = resource_map_.find(resource);
  if (found == resource_map_.end())
    return;
  found->second.ref_count--;
  if (found->second.ref_count == 0) {
    if (notify_browser_on_release) {
      dispatcher_->Send(new PpapiHostMsg_PPBCore_ReleaseResource(
          INTERFACE_ID_PPB_CORE, resource));
    }
    resource_map_.erase(found);
  }
}

}  // namespace proxy
}  // namespace pp

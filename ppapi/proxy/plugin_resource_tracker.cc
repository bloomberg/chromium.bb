// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/plugin_resource_tracker.h"

#include "base/logging.h"
#include "base/singleton.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/serialized_var.h"

namespace pp {
namespace proxy {

namespace {

// When non-NULL, this object overrides the ResourceTrackerSingleton.
PluginResourceTracker* g_resource_tracker_override = NULL;

}  // namespace

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

// Start counting resources at a high number to avoid collisions with vars (to
// help debugging).
PluginResourceTracker::PluginResourceTracker()
    : last_resource_id_(0x00100000) {
}

PluginResourceTracker::~PluginResourceTracker() {
}

// static
void PluginResourceTracker::SetInstanceForTest(PluginResourceTracker* tracker) {
  g_resource_tracker_override = tracker;
}

// static
PluginResourceTracker* PluginResourceTracker::GetInstance() {
  if (g_resource_tracker_override)
    return g_resource_tracker_override;
  return Singleton<PluginResourceTracker>::get();
}

PluginResource* PluginResourceTracker::GetResourceObject(
    PP_Resource pp_resource) {
  ResourceMap::iterator found = resource_map_.find(pp_resource);
  if (found == resource_map_.end())
    return NULL;
  return found->second.resource.get();
}

PP_Resource PluginResourceTracker::AddResource(
    linked_ptr<PluginResource> object) {
  if (object->host_resource().is_null()) {
    // Prevent adding null resources or GetResourceObject(0) will return a
    // valid pointer!
    NOTREACHED();
    return 0;
  }

  PP_Resource plugin_resource = ++last_resource_id_;
  DCHECK(resource_map_.find(plugin_resource) == resource_map_.end());
  resource_map_[plugin_resource] = ResourceInfo(1, object);
  host_resource_map_[object->host_resource()] = plugin_resource;
  return plugin_resource;
}

void PluginResourceTracker::AddRefResource(PP_Resource resource) {
  ResourceMap::iterator found = resource_map_.find(resource);
  if (found == resource_map_.end()) {
    NOTREACHED();
    return;
  }
  found->second.ref_count++;
}

void PluginResourceTracker::ReleaseResource(PP_Resource resource) {
  ReleasePluginResourceRef(resource, true);
}

PP_Resource PluginResourceTracker::PluginResourceForHostResource(
    const HostResource& resource) const {
  HostResourceMap::const_iterator found = host_resource_map_.find(resource);
  if (found == host_resource_map_.end())
    return 0;
  return found->second;
}

void PluginResourceTracker::ReleasePluginResourceRef(
    const PP_Resource& resource,
    bool notify_browser_on_release) {
  ResourceMap::iterator found = resource_map_.find(resource);
  if (found == resource_map_.end())
    return;
  found->second.ref_count--;
  if (found->second.ref_count == 0) {
    PluginResource* plugin_resource = found->second.resource.get();
    if (notify_browser_on_release)
      SendReleaseResourceToHost(resource, plugin_resource);
    host_resource_map_.erase(plugin_resource->host_resource());
    resource_map_.erase(found);
  }
}

void PluginResourceTracker::SendReleaseResourceToHost(
    PP_Resource resource_id,
    PluginResource* resource) {
  PluginDispatcher* dispatcher =
      PluginDispatcher::GetForInstance(resource->instance());
  if (dispatcher) {
    dispatcher->Send(new PpapiHostMsg_PPBCore_ReleaseResource(
        INTERFACE_ID_PPB_CORE, resource->host_resource()));
  } else {
    NOTREACHED();
  }
}

}  // namespace proxy
}  // namespace pp

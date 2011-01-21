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

PluginResourceTracker::PluginResourceTracker() {
}

PluginResourceTracker::~PluginResourceTracker() {
}

// static
PluginResourceTracker* PluginResourceTracker::GetInstance() {
  return Singleton<PluginResourceTracker>::get();
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

bool PluginResourceTracker::PreparePreviouslyTrackedResource(
    PP_Resource resource) {
  ResourceMap::iterator found = resource_map_.find(resource);
  if (found == resource_map_.end())
    return false;  // We've not seen this resource before.

  // We have already seen this resource and the caller wants the plugin to
  // have one more ref to the object (this function is called when retuning
  // a resource).
  //
  // This is like the PluginVarTracker::ReceiveObjectPassRef. We do an AddRef
  // in the plugin for the additional ref, and then a Release in the renderer
  // because the code in the renderer addrefed on behalf of the caller.
  found->second.ref_count++;

  SendReleaseResourceToHost(resource, found->second.resource.get());
  return true;
}

void PluginResourceTracker::ReleasePluginResourceRef(
    const PP_Resource& resource,
    bool notify_browser_on_release) {
  ResourceMap::iterator found = resource_map_.find(resource);
  if (found == resource_map_.end())
    return;
  found->second.ref_count--;
  if (found->second.ref_count == 0) {
    if (notify_browser_on_release)
      SendReleaseResourceToHost(resource, found->second.resource.get());
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
        INTERFACE_ID_PPB_CORE, resource_id));
  } else {
    NOTREACHED();
  }
}

}  // namespace proxy
}  // namespace pp

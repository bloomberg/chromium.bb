// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/plugin_resource_tracker.h"

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/serialized_var.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/shared_impl/tracker_base.h"
#include "ppapi/shared_impl/var.h"

namespace ppapi {
namespace proxy {

namespace {

// When non-NULL, this object overrides the ResourceTrackerSingleton.
PluginResourceTracker* g_resource_tracker_override = NULL;

TrackerBase* GetTrackerBase() {
  return PluginResourceTracker::GetInstance();
}

}  // namespace

PluginResourceTracker::PluginResourceTracker()
    : var_tracker_test_override_(NULL) {
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

// static
TrackerBase* PluginResourceTracker::GetTrackerBaseInstance() {
  return GetInstance();
}

PP_Resource PluginResourceTracker::PluginResourceForHostResource(
    const HostResource& resource) const {
  HostResourceMap::const_iterator found = host_resource_map_.find(resource);
  if (found == host_resource_map_.end())
    return 0;
  return found->second;
}

FunctionGroupBase* PluginResourceTracker::GetFunctionAPI(PP_Instance inst,
                                                         InterfaceID id) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(inst);
  if (dispatcher)
    return dispatcher->GetFunctionAPI(id);
  return NULL;
}

VarTracker* PluginResourceTracker::GetVarTracker() {
  return &var_tracker();
}

ResourceTracker* PluginResourceTracker::GetResourceTracker() {
  return this;
}

PP_Module PluginResourceTracker::GetModuleForInstance(PP_Instance instance) {
  // Currently proxied plugins don't use the PP_Module for anything useful.
  return 0;
}

PP_Resource PluginResourceTracker::AddResource(Resource* object) {
  PP_Resource ret = ResourceTracker::AddResource(object);

  // Some resources are plugin-only, so they don't have a host resource.
  if (object->host_resource().host_resource())
    host_resource_map_.insert(std::make_pair(object->host_resource(), ret));
  return ret;
}

void PluginResourceTracker::RemoveResource(Resource* object) {
  ResourceTracker::RemoveResource(object);

  if (!object->host_resource().is_null()) {
    // The host_resource will be NULL for proxy-only resources, which we
    // obviously don't need to tell the host about.
    DCHECK(host_resource_map_.find(object->host_resource()) !=
           host_resource_map_.end());
    host_resource_map_.erase(object->host_resource());

    PluginDispatcher* dispatcher =
        PluginDispatcher::GetForInstance(object->pp_instance());
    if (dispatcher) {
      // The dispatcher can be NULL if the plugin held on to a resource after
      // the instance was destroyed. In that case the browser-side resource has
      // already been freed correctly on the browser side.
      dispatcher->Send(new PpapiHostMsg_PPBCore_ReleaseResource(
          INTERFACE_ID_PPB_CORE, object->host_resource()));
    }
  }
}

}  // namespace proxy
}  // namespace ppapi

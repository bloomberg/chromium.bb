// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/resource_tracker.h"

#include "ppapi/shared_impl/id_assignment.h"
#include "ppapi/shared_impl/resource.h"

namespace ppapi {

ResourceTracker::ResourceTracker() : last_resource_value_(0) {
}

ResourceTracker::~ResourceTracker() {
}

Resource* ResourceTracker::GetResource(PP_Resource res) const {
  ResourceMap::const_iterator i = live_resources_.find(res);
  if (i == live_resources_.end())
    return NULL;
  return i->second.first;
}

void ResourceTracker::AddRefResource(PP_Resource res) {
  DLOG_IF(ERROR, !CheckIdType(res, PP_ID_TYPE_RESOURCE))
      << res << " is not a PP_Resource.";
  ResourceMap::iterator i = live_resources_.find(res);
  if (i == live_resources_.end())
    return;

  // Prevent overflow of refcount.
  if (i->second.second ==
      std::numeric_limits<ResourceAndRefCount::second_type>::max())
    return;

  // When we go from 0 to 1 plugin ref count, keep an additional "real" ref
  // on its behalf.
  if (i->second.second == 0)
    i->second.first->AddRef();

  i->second.second++;
  return;
}

void ResourceTracker::ReleaseResource(PP_Resource res) {
  DLOG_IF(ERROR, !CheckIdType(res, PP_ID_TYPE_RESOURCE))
      << res << " is not a PP_Resource.";
  ResourceMap::iterator i = live_resources_.find(res);
  if (i == live_resources_.end())
    return;

  // Prevent underflow of refcount.
  if (i->second.second == 0)
    return;

  i->second.second--;
  if (i->second.second == 0) {
    LastPluginRefWasDeleted(i->second.first);

    // When we go from 1 to 0 plugin ref count, free the additional "real" ref
    // on its behalf. THIS WILL MOST LIKELY RELEASE THE OBJECT AND REMOVE IT
    // FROM OUR LIST.
    i->second.first->Release();
  }
}

void ResourceTracker::DidCreateInstance(PP_Instance instance) {
  // Due to the infrastructure of some tests, the instance is registered
  // twice in a few cases. It would be nice not to do that and assert here
  // instead.
  if (instance_map_.find(instance) != instance_map_.end())
    return;
  instance_map_[instance] = linked_ptr<InstanceData>(new InstanceData);
}

void ResourceTracker::DidDeleteInstance(PP_Instance instance) {
  InstanceMap::iterator found_instance = instance_map_.find(instance);

  // Due to the infrastructure of some tests, the instance is uyregistered
  // twice in a few cases. It would be nice not to do that and assert here
  // instead.
  if (found_instance == instance_map_.end())
    return;

  InstanceData& data = *found_instance->second;

  // Force release all plugin references to resources associated with the
  // deleted instance. Make a copy since as we iterate through them, each one
  // will remove itself from the tracking info individually.
  ResourceSet to_delete = data.resources;
  ResourceSet::iterator cur = to_delete.begin();
  while (cur != to_delete.end()) {
    // Note that it's remotely possible for the object to already be deleted
    // from the live resources. One case is if a resource object is holding
    // the last ref to another. When we release the first one, it will release
    // the second one. So the second one will be gone when we eventually get
    // to it.
    ResourceMap::iterator found_resource = live_resources_.find(*cur);
    if (found_resource != live_resources_.end()) {
      Resource* resource = found_resource->second.first;
      if (found_resource->second.second > 0) {
        LastPluginRefWasDeleted(resource);
        found_resource->second.second = 0;

        // This will most likely delete the resource object and remove it
        // from the live_resources_ list.
        resource->Release();
      }
    }

    cur++;
  }

  // In general the above pass will delete all the resources and there won't
  // be any left in the map. However, if parts of the implementation are still
  // holding on to internal refs, we need to tell them that the instance is
  // gone.
  to_delete = data.resources;
  cur = to_delete.begin();
  while (cur != to_delete.end()) {
    ResourceMap::iterator found_resource = live_resources_.find(*cur);
    if (found_resource != live_resources_.end())
      found_resource->second.first->InstanceWasDeleted();
    cur++;
  }

  instance_map_.erase(instance);
}

int ResourceTracker::GetLiveObjectsForInstance(PP_Instance instance) const {
  InstanceMap::const_iterator found = instance_map_.find(instance);
  if (found == instance_map_.end())
    return 0;
  return static_cast<int>(found->second->resources.size());
}

PP_Resource ResourceTracker::AddResource(Resource* object) {
  // If the plugin manages to create too many resources, don't do crazy stuff.
  if (last_resource_value_ == kMaxPPId)
    return 0;

  // If you hit this somebody forgot to call DidCreateInstance or the resource
  // was created with an invalid PP_Instance.
  //
  // This is specifically a check even in release mode. When creating resources
  // it can be easy to forget to validate the instance parameter. If somebody
  // does forget, we don't want to introduce a vulnerability with invalid
  // pointers floating around, so we die ASAP.
  InstanceMap::iterator found = instance_map_.find(object->pp_instance());
  CHECK(found != instance_map_.end());

  PP_Resource new_id = MakeTypedId(++last_resource_value_, PP_ID_TYPE_RESOURCE);
  found->second->resources.insert(new_id);

  live_resources_[new_id] = ResourceAndRefCount(object, 0);
  return new_id;
}

void ResourceTracker::RemoveResource(Resource* object) {
  PP_Resource pp_resource = object->pp_resource();
  if (object->pp_instance())
    instance_map_[object->pp_instance()]->resources.erase(pp_resource);
  live_resources_.erase(pp_resource);
}

void ResourceTracker::LastPluginRefWasDeleted(Resource* object) {
  object->LastPluginRefWasDeleted();
}

}  // namespace ppapi

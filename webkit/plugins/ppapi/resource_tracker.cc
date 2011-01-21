// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/resource_tracker.h"

#include <limits>
#include <set>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "ppapi/c/pp_resource.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/resource.h"
#include "webkit/plugins/ppapi/var.h"

enum PPIdType {
  PP_ID_TYPE_MODULE,
  PP_ID_TYPE_INSTANCE,
  PP_ID_TYPE_RESOURCE,
  PP_ID_TYPE_VAR,
  PP_ID_TYPE_COUNT
};

static const unsigned int kPPIdTypeBits = 2;
COMPILE_ASSERT(PP_ID_TYPE_COUNT <= (1<<kPPIdTypeBits),
               kPPIdTypeBits_is_too_small_for_all_id_types);

template <typename T> static inline T MakeTypedId(T value, PPIdType type) {
  return (value << kPPIdTypeBits) | static_cast<T>(type);
}

template <typename T> static inline bool CheckIdType(T id, PPIdType type) {
  // 0 is a valid resource.
  if (!id)
    return true;
  const T mask = (static_cast<T>(1) << kPPIdTypeBits) - 1;
  return (id & mask) == type;
}

namespace webkit {
namespace ppapi {

static base::LazyInstance<ResourceTracker> g_resource_tracker(
    base::LINKER_INITIALIZED);

scoped_refptr<Resource> ResourceTracker::GetResource(PP_Resource res) const {
  DLOG_IF(ERROR, !CheckIdType(res, PP_ID_TYPE_RESOURCE))
      << res << " is not a PP_Resource.";
  ResourceMap::const_iterator result = live_resources_.find(res);
  if (result == live_resources_.end()) {
    return scoped_refptr<Resource>();
  }
  return result->second.first;
}

// static
ResourceTracker* ResourceTracker::singleton_override_ = NULL;

ResourceTracker::ResourceTracker()
    : last_resource_id_(0),
      last_var_id_(0) {
}

ResourceTracker::~ResourceTracker() {
}

// static
ResourceTracker* ResourceTracker::Get() {
  if (singleton_override_)
    return singleton_override_;
  return g_resource_tracker.Pointer();
}

PP_Resource ResourceTracker::AddResource(Resource* resource) {
  // If the plugin manages to create 1 billion resources, don't do crazy stuff.
  if (last_resource_id_ ==
      (std::numeric_limits<PP_Resource>::max() >> kPPIdTypeBits))
    return 0;

  // Add the resource with plugin use-count 1.
  PP_Resource new_id = MakeTypedId(++last_resource_id_, PP_ID_TYPE_RESOURCE);
  live_resources_.insert(std::make_pair(new_id, std::make_pair(resource, 1)));
  instance_to_resources_[resource->instance()->pp_instance()].insert(new_id);
  return new_id;
}

int32 ResourceTracker::AddVar(Var* var) {
  // If the plugin manages to create 1B strings...
  if (last_var_id_ == std::numeric_limits<int32>::max() >> kPPIdTypeBits) {
    return 0;
  }
  // Add the resource with plugin use-count 1.
  int32 new_id = MakeTypedId(++last_var_id_, PP_ID_TYPE_VAR);
  live_vars_.insert(std::make_pair(new_id, std::make_pair(var, 1)));
  return new_id;
}

bool ResourceTracker::AddRefResource(PP_Resource res) {
  DLOG_IF(ERROR, !CheckIdType(res, PP_ID_TYPE_RESOURCE))
      << res << " is not a PP_Resource.";
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
  DLOG_IF(ERROR, !CheckIdType(res, PP_ID_TYPE_RESOURCE))
      << res << " is not a PP_Resource.";
  ResourceMap::iterator i = live_resources_.find(res);
  if (i != live_resources_.end()) {
    if (!--i->second.second) {
      Resource* to_release = i->second.first;
      to_release->LastPluginRefWasDeleted(false);

      ResourceSet& instance_resource_set =
          instance_to_resources_[to_release->instance()->pp_instance()];
      DCHECK(instance_resource_set.find(res) != instance_resource_set.end());
      instance_resource_set.erase(res);

      live_resources_.erase(i);
    }
    return true;
  } else {
    return false;
  }
}

void ResourceTracker::ForceDeletePluginResourceRefs(PP_Resource res) {
  DLOG_IF(ERROR, !CheckIdType(res, PP_ID_TYPE_RESOURCE))
      << res << " is not a PP_Resource.";
  ResourceMap::iterator i = live_resources_.find(res);
  if (i == live_resources_.end())
    return;  // Nothing to do.

  i->second.second = 0;
  Resource* resource = i->second.first;

  // Must delete from the resource set first since the resource's instance
  // pointer will get zeroed out in LastPluginRefWasDeleted.
  ResourceSet& resource_set = instance_to_resources_[
      resource->instance()->pp_instance()];
  DCHECK(resource_set.find(res) != resource_set.end());
  resource_set.erase(res);

  resource->LastPluginRefWasDeleted(true);
  live_resources_.erase(i);
}

uint32 ResourceTracker::GetLiveObjectsForInstance(
    PP_Instance instance) const {
  InstanceToResourceMap::const_iterator found =
      instance_to_resources_.find(instance);
  if (found == instance_to_resources_.end())
    return 0;
  return static_cast<uint32>(found->second.size());
}

scoped_refptr<Var> ResourceTracker::GetVar(int32 var_id) const {
  DLOG_IF(ERROR, !CheckIdType(var_id, PP_ID_TYPE_VAR))
      << var_id << " is not a PP_Var ID.";
  VarMap::const_iterator result = live_vars_.find(var_id);
  if (result == live_vars_.end()) {
    return scoped_refptr<Var>();
  }
  return result->second.first;
}

bool ResourceTracker::AddRefVar(int32 var_id) {
  DLOG_IF(ERROR, !CheckIdType(var_id, PP_ID_TYPE_VAR))
      << var_id << " is not a PP_Var ID.";
  VarMap::iterator i = live_vars_.find(var_id);
  if (i != live_vars_.end()) {
    // We don't protect against overflow, since a plugin as malicious as to ref
    // once per every byte in the address space could have just as well unrefed
    // one time too many.
    ++i->second.second;
    return true;
  }
  return false;
}

bool ResourceTracker::UnrefVar(int32 var_id) {
  DLOG_IF(ERROR, !CheckIdType(var_id, PP_ID_TYPE_VAR))
      << var_id << " is not a PP_Var ID.";
  VarMap::iterator i = live_vars_.find(var_id);
  if (i != live_vars_.end()) {
    if (!--i->second.second)
      live_vars_.erase(i);
    return true;
  }
  return false;
}

PP_Instance ResourceTracker::AddInstance(PluginInstance* instance) {
#ifndef NDEBUG
  // Make sure we're not adding one more than once.
  for (InstanceMap::const_iterator i = instance_map_.begin();
       i != instance_map_.end(); ++i)
    DCHECK(i->second != instance);
#endif

  // Use a random 64-bit number for the instance ID. This helps prevent some
  // mischeif where you could misallocate resources if you gave a different
  // instance ID.
  //
  // See also AddModule below.
  //
  // Need to make sure the random number isn't a duplicate or 0.
  PP_Instance new_instance;
  do {
    new_instance = MakeTypedId(static_cast<PP_Instance>(base::RandUint64()),
                               PP_ID_TYPE_INSTANCE);
  } while (!new_instance ||
           instance_map_.find(new_instance) != instance_map_.end());
  instance_map_[new_instance] = instance;
  return new_instance;
}

void ResourceTracker::InstanceDeleted(PP_Instance instance) {
  DLOG_IF(ERROR, !CheckIdType(instance, PP_ID_TYPE_INSTANCE))
      << instance << " is not a PP_Instance.";
  // Force release all plugin references to resources associated with the
  // deleted instance.
  ResourceSet& resource_set = instance_to_resources_[instance];
  ResourceSet::iterator i = resource_set.begin();
  while (i != resource_set.end()) {
    // Iterators to a set are stable so we can iterate the set while the items
    // are being deleted as long as we're careful not to delete the item we're
    // holding an iterator to.
    ResourceSet::iterator current = i++;
    ForceDeletePluginResourceRefs(*current);
  }
  DCHECK(resource_set.empty());
  instance_to_resources_.erase(instance);

  InstanceMap::iterator found = instance_map_.find(instance);
  if (found == instance_map_.end()) {
    NOTREACHED();
    return;
  }
  instance_map_.erase(found);
}

PluginInstance* ResourceTracker::GetInstance(PP_Instance instance) {
  DLOG_IF(ERROR, !CheckIdType(instance, PP_ID_TYPE_INSTANCE))
      << instance << " is not a PP_Instance.";
  InstanceMap::iterator found = instance_map_.find(instance);
  if (found == instance_map_.end())
    return NULL;
  return found->second;
}

PP_Module ResourceTracker::AddModule(PluginModule* module) {
#ifndef NDEBUG
  // Make sure we're not adding one more than once.
  for (ModuleMap::const_iterator i = module_map_.begin();
       i != module_map_.end(); ++i)
    DCHECK(i->second != module);
#endif

  // See AddInstance above.
  PP_Module new_module;
  do {
    new_module = MakeTypedId(static_cast<PP_Module>(base::RandUint64()),
                             PP_ID_TYPE_MODULE);
  } while (!new_module ||
           module_map_.find(new_module) != module_map_.end());
  module_map_[new_module] = module;
  return new_module;
}

void ResourceTracker::ModuleDeleted(PP_Module module) {
  DLOG_IF(ERROR, !CheckIdType(module, PP_ID_TYPE_MODULE))
      << module << " is not a PP_Module.";
  ModuleMap::iterator found = module_map_.find(module);
  if (found == module_map_.end()) {
    NOTREACHED();
    return;
  }
  module_map_.erase(found);
}

PluginModule* ResourceTracker::GetModule(PP_Module module) {
  DLOG_IF(ERROR, !CheckIdType(module, PP_ID_TYPE_MODULE))
      << module << " is not a PP_Module.";
  ModuleMap::iterator found = module_map_.find(module);
  if (found == module_map_.end())
    return NULL;
  return found->second;
}

// static
void ResourceTracker::SetSingletonOverride(ResourceTracker* tracker) {
  DCHECK(!singleton_override_);
  singleton_override_ = tracker;
}

// static
void ResourceTracker::ClearSingletonOverride() {
  DCHECK(singleton_override_);
  singleton_override_ = NULL;
}

}  // namespace ppapi
}  // namespace webkit


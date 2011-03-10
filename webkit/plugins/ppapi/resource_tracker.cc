// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/resource_tracker.h"

#include <limits>
#include <set>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_var.h"
#include "webkit/plugins/ppapi/plugin_module.h"
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

struct ResourceTracker::InstanceData {
  InstanceData() : instance(0) {}

  // Non-owning pointer to the instance object. When a PluginInstance is
  // destroyed, it will notify us and we'll delete all associated data.
  PluginInstance* instance;

  // Resources and object vars associated with the instance.
  ResourceSet resources;
  VarSet object_vars;
};

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

  // Track associated with the instance.
  PP_Instance pp_instance = resource->instance()->pp_instance();
  DCHECK(instance_map_.find(pp_instance) != instance_map_.end());
  instance_map_[pp_instance].resources.insert(new_id);
  return new_id;
}

int32 ResourceTracker::AddVar(Var* var) {
  // If the plugin manages to create 1B strings...
  if (last_var_id_ == std::numeric_limits<int32>::max() >> kPPIdTypeBits)
    return 0;

  // Add the resource with plugin use-count 1.
  int32 new_id = MakeTypedId(++last_var_id_, PP_ID_TYPE_VAR);
  live_vars_.insert(std::make_pair(new_id, std::make_pair(var, 1)));

  // Object vars must be associated with the instance.
  ObjectVar* object_var = var->AsObjectVar();
  if (object_var) {
    PP_Instance instance = object_var->instance()->pp_instance();
    DCHECK(instance_map_.find(instance) != instance_map_.end());
    instance_map_[instance].object_vars.insert(new_id);
  }

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
      // LastPluginRefWasDeleted will clear the instance pointer, so save it
      // first.
      PP_Instance instance = to_release->instance()->pp_instance();
      to_release->LastPluginRefWasDeleted(false);

      instance_map_[instance].resources.erase(res);
      live_resources_.erase(i);
    }
    return true;
  } else {
    return false;
  }
}

void ResourceTracker::CleanupInstanceData(PP_Instance instance,
                                          bool delete_instance) {
  DLOG_IF(ERROR, !CheckIdType(instance, PP_ID_TYPE_INSTANCE))
      << instance << " is not a PP_Instance.";
  InstanceMap::iterator found = instance_map_.find(instance);
  if (found == instance_map_.end()) {
    NOTREACHED();
    return;
  }
  InstanceData& data = found->second;

  // Force release all plugin references to resources associated with the
  // deleted instance.
  ResourceSet::iterator cur_res = data.resources.begin();
  while (cur_res != data.resources.end()) {
    ResourceMap::iterator found_resource = live_resources_.find(*cur_res);
    if (found_resource == live_resources_.end()) {
      NOTREACHED();
    } else {
      Resource* resource = found_resource->second.first;

      // Must delete from the resource set first since the resource's instance
      // pointer will get zeroed out in LastPluginRefWasDeleted.
      resource->LastPluginRefWasDeleted(true);
      live_resources_.erase(*cur_res);
    }

    // Iterators to a set are stable so we can iterate the set while the items
    // are being deleted as long as we're careful not to delete the item we're
    // holding an iterator to.
    ResourceSet::iterator current = cur_res++;
    data.resources.erase(current);
  }
  DCHECK(data.resources.empty());

  // Force delete all var references.
  VarSet::iterator cur_var = data.object_vars.begin();
  while (cur_var != data.object_vars.end()) {
    VarSet::iterator current = cur_var++;

    // Tell the corresponding ObjectVar that the instance is gone.
    PP_Var object_pp_var;
    object_pp_var.type = PP_VARTYPE_OBJECT;
    object_pp_var.value.as_id = *current;
    scoped_refptr<ObjectVar> object_var(ObjectVar::FromPPVar(object_pp_var));
    if (object_var.get())
      object_var->InstanceDeleted();

    // Clear the object from the var mapping and the live instance object list.
    live_vars_.erase(*current);
    data.object_vars.erase(*current);
  }
  DCHECK(data.object_vars.empty());

  if (delete_instance)
    instance_map_.erase(found);
}

uint32 ResourceTracker::GetLiveObjectsForInstance(
    PP_Instance instance) const {
  InstanceMap::const_iterator found = instance_map_.find(instance);
  if (found == instance_map_.end())
    return 0;
  return static_cast<uint32>(found->second.resources.size() +
                             found->second.object_vars.size());
}

scoped_refptr<Var> ResourceTracker::GetVar(int32 var_id) const {
  DLOG_IF(ERROR, !CheckIdType(var_id, PP_ID_TYPE_VAR))
      << var_id << " is not a PP_Var ID.";
  VarMap::const_iterator result = live_vars_.find(var_id);
  if (result == live_vars_.end())
    return scoped_refptr<Var>();
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
    if (!--i->second.second) {
      ObjectVar* object_var = i->second.first->AsObjectVar();
      if (object_var) {
        instance_map_[object_var->instance()->pp_instance()].object_vars.erase(
            var_id);
      }
      live_vars_.erase(i);
    }
    return true;
  }
  return false;
}

PP_Instance ResourceTracker::AddInstance(PluginInstance* instance) {
  DCHECK(instance_map_.find(instance->pp_instance()) == instance_map_.end());

  // Use a random number for the instance ID. This helps prevent some
  // accidents. See also AddModule below.
  //
  // Need to make sure the random number isn't a duplicate or 0.
  PP_Instance new_instance;
  do {
    new_instance = MakeTypedId(static_cast<PP_Instance>(base::RandUint64()),
                               PP_ID_TYPE_INSTANCE);
  } while (!new_instance ||
           instance_map_.find(new_instance) != instance_map_.end() ||
           !instance->module()->ReserveInstanceID(new_instance));

  instance_map_[new_instance].instance = instance;
  return new_instance;
}

void ResourceTracker::InstanceDeleted(PP_Instance instance) {
  CleanupInstanceData(instance, true);
}

void ResourceTracker::InstanceCrashed(PP_Instance instance) {
  CleanupInstanceData(instance, false);
}

PluginInstance* ResourceTracker::GetInstance(PP_Instance instance) {
  DLOG_IF(ERROR, !CheckIdType(instance, PP_ID_TYPE_INSTANCE))
      << instance << " is not a PP_Instance.";
  InstanceMap::iterator found = instance_map_.find(instance);
  if (found == instance_map_.end())
    return NULL;
  return found->second.instance;
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


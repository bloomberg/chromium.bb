// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/pepper_resource_tracker.h"

#include <limits>
#include <set>

#include "base/logging.h"
#include "base/rand_util.h"
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

ResourceTracker::ResourceTracker()
    : last_id_(0) {
}

ResourceTracker::~ResourceTracker() {
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

uint32 ResourceTracker::GetLiveObjectsForModule(PluginModule* module) const {
  // Since this is for testing only, we'll just go through all of them and
  // count.
  //
  // TODO(brettw) we will eventually need to implement more efficient
  // module->resource lookup to free resources when a module is unloaded. In
  // this case, this function can be implemented using that system.
  uint32 count = 0;
  for (ResourceMap::const_iterator i = live_resources_.begin();
       i != live_resources_.end(); ++i)
    count++;
  return count;
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
    new_instance = static_cast<PP_Instance>(base::RandUint64());
  } while (!new_instance ||
           instance_map_.find(new_instance) != instance_map_.end());
  instance_map_[new_instance] = instance;
  return new_instance;
}

void ResourceTracker::InstanceDeleted(PP_Instance instance) {
  InstanceMap::iterator found = instance_map_.find(instance);
  if (found == instance_map_.end()) {
    NOTREACHED();
    return;
  }
  instance_map_.erase(found);
}

PluginInstance* ResourceTracker::GetInstance(PP_Instance instance) {
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
    new_module = static_cast<PP_Module>(base::RandUint64());
  } while (!new_module ||
           module_map_.find(new_module) != module_map_.end());
  module_map_[new_module] = module;
  return new_module;
}

void ResourceTracker::ModuleDeleted(PP_Module module) {
  ModuleMap::iterator found = module_map_.find(module);
  if (found == module_map_.end()) {
    NOTREACHED();
    return;
  }
  module_map_.erase(found);
}

PluginModule* ResourceTracker::GetModule(PP_Module module) {
  ModuleMap::iterator found = module_map_.find(module);
  if (found == module_map_.end())
    return NULL;
  return found->second;
}

}  // namespace pepper

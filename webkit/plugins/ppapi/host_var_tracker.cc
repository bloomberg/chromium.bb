// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/host_var_tracker.h"

#include "base/logging.h"
#include "ppapi/c/pp_var.h"
#include "webkit/plugins/ppapi/host_array_buffer_var.h"
#include "webkit/plugins/ppapi/npobject_var.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"

using ppapi::ArrayBufferVar;
using ppapi::NPObjectVar;

namespace webkit {
namespace ppapi {

HostVarTracker::HostVarTracker()
  : VarTracker(SINGLE_THREADED),
    last_shared_memory_map_id_(0) {
}

HostVarTracker::~HostVarTracker() {
}

ArrayBufferVar* HostVarTracker::CreateArrayBuffer(uint32 size_in_bytes) {
  return new HostArrayBufferVar(size_in_bytes);
}

ArrayBufferVar* HostVarTracker::CreateShmArrayBuffer(
    uint32 size_in_bytes,
    base::SharedMemoryHandle handle) {
  return new HostArrayBufferVar(size_in_bytes, handle);
}

void HostVarTracker::AddNPObjectVar(NPObjectVar* object_var) {
  CheckThreadingPreconditions();

  InstanceMap::iterator found_instance = instance_map_.find(
      object_var->pp_instance());
  if (found_instance == instance_map_.end()) {
    // Lazily create the instance map.
    DCHECK(object_var->pp_instance() != 0);
    found_instance = instance_map_.insert(std::make_pair(
        object_var->pp_instance(),
        linked_ptr<NPObjectToNPObjectVarMap>(new NPObjectToNPObjectVarMap))).
            first;
  }
  NPObjectToNPObjectVarMap* np_object_map = found_instance->second.get();

  DCHECK(np_object_map->find(object_var->np_object()) ==
         np_object_map->end()) << "NPObjectVar already in map";
  np_object_map->insert(std::make_pair(object_var->np_object(), object_var));
}

void HostVarTracker::RemoveNPObjectVar(NPObjectVar* object_var) {
  CheckThreadingPreconditions();

  InstanceMap::iterator found_instance = instance_map_.find(
      object_var->pp_instance());
  if (found_instance == instance_map_.end()) {
    NOTREACHED() << "NPObjectVar has invalid instance.";
    return;
  }
  NPObjectToNPObjectVarMap* np_object_map = found_instance->second.get();

  NPObjectToNPObjectVarMap::iterator found_object =
      np_object_map->find(object_var->np_object());
  if (found_object == np_object_map->end()) {
    NOTREACHED() << "NPObjectVar not registered.";
    return;
  }
  if (found_object->second != object_var) {
    NOTREACHED() << "NPObjectVar doesn't match.";
    return;
  }
  np_object_map->erase(found_object);
}

NPObjectVar* HostVarTracker::NPObjectVarForNPObject(PP_Instance instance,
                                                    NPObject* np_object) {
  CheckThreadingPreconditions();

  InstanceMap::iterator found_instance = instance_map_.find(instance);
  if (found_instance == instance_map_.end())
    return NULL;  // No such instance.
  NPObjectToNPObjectVarMap* np_object_map = found_instance->second.get();

  NPObjectToNPObjectVarMap::iterator found_object =
      np_object_map->find(np_object);
  if (found_object == np_object_map->end())
    return NULL;  // No such object.
  return found_object->second;
}

int HostVarTracker::GetLiveNPObjectVarsForInstance(PP_Instance instance) const {
  CheckThreadingPreconditions();

  InstanceMap::const_iterator found = instance_map_.find(instance);
  if (found == instance_map_.end())
    return 0;
  return static_cast<int>(found->second->size());
}

void HostVarTracker::DidDeleteInstance(PP_Instance instance) {
  CheckThreadingPreconditions();

  InstanceMap::iterator found_instance = instance_map_.find(instance);
  if (found_instance == instance_map_.end())
    return;  // Nothing to do.
  NPObjectToNPObjectVarMap* np_object_map = found_instance->second.get();

  // Force delete all var references. ForceReleaseNPObject() will cause
  // this object, and potentially others it references, to be removed from
  // |np_object_map|.
  while (!np_object_map->empty()) {
    ForceReleaseNPObject(np_object_map->begin()->second);
  }

  // Remove the record for this instance since it should be empty.
  DCHECK(np_object_map->empty());
  instance_map_.erase(found_instance);
}

void HostVarTracker::ForceReleaseNPObject(::ppapi::NPObjectVar* object_var) {
  object_var->InstanceDeleted();
  VarMap::iterator iter = live_vars_.find(object_var->GetExistingVarID());
  if (iter == live_vars_.end()) {
    NOTREACHED();
    return;
  }
  iter->second.ref_count = 0;
  DCHECK(iter->second.track_with_no_reference_count == 0);
  DeleteObjectInfoIfNecessary(iter);
}

int HostVarTracker::TrackSharedMemoryHandle(PP_Instance instance,
                                            base::SharedMemoryHandle handle,
                                            uint32 size_in_bytes) {
  SharedMemoryMapEntry entry;
  entry.instance = instance;
  entry.handle = handle;
  entry.size_in_bytes = size_in_bytes;

  // Find a free id for our map.
  while (shared_memory_map_.find(last_shared_memory_map_id_) !=
         shared_memory_map_.end()) {
    ++last_shared_memory_map_id_;
  }
  shared_memory_map_[last_shared_memory_map_id_] = entry;
  return last_shared_memory_map_id_;
}

bool HostVarTracker::StopTrackingSharedMemoryHandle(
    int id,
    PP_Instance instance,
    base::SharedMemoryHandle* handle,
    uint32* size_in_bytes) {
  SharedMemoryMap::iterator it = shared_memory_map_.find(id);
  if (it == shared_memory_map_.end())
    return false;
  if (it->second.instance != instance)
    return false;

  *handle = it->second.handle;
  *size_in_bytes = it->second.size_in_bytes;
  shared_memory_map_.erase(it);
  return true;
}

}  // namespace ppapi
}  // namespace webkit

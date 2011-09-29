// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/resource_tracker.h"

#include <limits>
#include <set>

#include "base/logging.h"
#include "base/rand_util.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/shared_impl/function_group_base.h"
#include "ppapi/shared_impl/id_assignment.h"
#include "ppapi/shared_impl/tracker_base.h"
#include "webkit/plugins/ppapi/callbacks.h"
#include "webkit/plugins/ppapi/npobject_var.h"
#include "webkit/plugins/ppapi/plugin_module.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/ppb_char_set_impl.h"
#include "webkit/plugins/ppapi/ppb_cursor_control_impl.h"
#include "webkit/plugins/ppapi/ppb_font_impl.h"
#include "webkit/plugins/ppapi/ppb_text_input_impl.h"
#include "webkit/plugins/ppapi/resource_creation_impl.h"
#include "webkit/plugins/ppapi/resource_helper.h"

using ppapi::CheckIdType;
using ppapi::MakeTypedId;
using ppapi::NPObjectVar;
using ppapi::PPIdType;
using ppapi::Var;

namespace webkit {
namespace ppapi {

namespace {

::ppapi::TrackerBase* GetTrackerBase() {
  return ResourceTracker::Get();
}

}  // namespace

typedef std::map<NPObject*, NPObjectVar*> NPObjectToNPObjectVarMap;

struct ResourceTracker::InstanceData {
  InstanceData() : instance(0) {}

  // Non-owning pointer to the instance object. When a PluginInstance is
  // destroyed, it will notify us and we'll delete all associated data.
  PluginInstance* instance;

  // Tracks all live NPObjectVars used by this module so we can map NPObjects
  // to the corresponding object, and also release these properly if the
  // instance goes away when there are still refs. These are non-owning
  // references.
  NPObjectToNPObjectVarMap np_object_to_object_var;

  // Lazily allocated function proxies for the different interfaces.
  scoped_ptr< ::ppapi::FunctionGroupBase >
      function_proxies[::ppapi::proxy::INTERFACE_ID_COUNT];
};

// static
ResourceTracker* ResourceTracker::global_tracker_ = NULL;
ResourceTracker* ResourceTracker::singleton_override_ = NULL;

ResourceTracker::ResourceTracker() {
  // Wire up the new shared resource tracker base to use our implementation.
  ::ppapi::TrackerBase::Init(&GetTrackerBase);
}

ResourceTracker::~ResourceTracker() {
}

// static
ResourceTracker* ResourceTracker::Get() {
  if (singleton_override_)
    return singleton_override_;
  if (!global_tracker_)
    global_tracker_ = new ResourceTracker;
  return global_tracker_;
}

void ResourceTracker::CleanupInstanceData(PP_Instance instance,
                                          bool delete_instance) {
  DLOG_IF(ERROR, !CheckIdType(instance, ::ppapi::PP_ID_TYPE_INSTANCE))
      << instance << " is not a PP_Instance.";
  InstanceMap::iterator found = instance_map_.find(instance);
  if (found == instance_map_.end()) {
    NOTREACHED();
    return;
  }
  InstanceData& data = *found->second;

  // Force delete all var references. Need to make a copy so we can iterate over
  // the map while deleting stuff from it.
  NPObjectToNPObjectVarMap np_object_map_copy = data.np_object_to_object_var;
  NPObjectToNPObjectVarMap::iterator cur_var =
      np_object_map_copy.begin();
  while (cur_var != np_object_map_copy.end()) {
    NPObjectToNPObjectVarMap::iterator current = cur_var++;

    // Clear the object from the var mapping and the live instance object list.
    int32 var_id = current->second->GetExistingVarID();
    if (var_id)
      live_vars_.erase(var_id);

    current->second->InstanceDeleted();
    data.np_object_to_object_var.erase(current->first);
  }
  DCHECK(data.np_object_to_object_var.empty());

  if (delete_instance)
    instance_map_.erase(found);
}

::ppapi::FunctionGroupBase* ResourceTracker::GetFunctionAPI(
    PP_Instance pp_instance,
    ::ppapi::proxy::InterfaceID id) {
  // Get the instance object. This also ensures that the instance data is in
  // the map, since we need it below.
  PluginInstance* instance = GetInstance(pp_instance);
  if (!instance)
    return NULL;

  // The instance one is special, since it's just implemented by the instance
  // object.
  if (id == ::ppapi::proxy::INTERFACE_ID_PPB_INSTANCE)
    return instance;

  scoped_ptr< ::ppapi::FunctionGroupBase >& proxy =
      instance_map_[pp_instance]->function_proxies[id];
  if (proxy.get())
    return proxy.get();

  switch (id) {
    case ::ppapi::proxy::INTERFACE_ID_PPB_CHAR_SET:
      proxy.reset(new PPB_CharSet_Impl(instance));
      break;
    case ::ppapi::proxy::INTERFACE_ID_PPB_CURSORCONTROL:
      proxy.reset(new PPB_CursorControl_Impl(instance));
      break;
    case ::ppapi::proxy::INTERFACE_ID_PPB_FONT:
      proxy.reset(new PPB_Font_FunctionImpl(instance));
      break;
    case ::ppapi::proxy::INTERFACE_ID_PPB_TEXT_INPUT:
      proxy.reset(new PPB_TextInput_Impl(instance));
      break;
    case ::ppapi::proxy::INTERFACE_ID_RESOURCE_CREATION:
      proxy.reset(new ResourceCreationImpl(instance));
      break;
    default:
      NOTREACHED();
  }

  return proxy.get();
}

::ppapi::VarTracker* ResourceTracker::GetVarTracker() {
  return &var_tracker_;
}

::ppapi::ResourceTracker* ResourceTracker::GetResourceTracker() {
  return this;
}

PP_Module ResourceTracker::GetModuleForInstance(PP_Instance instance) {
  PluginInstance* inst = GetInstance(instance);
  if (!inst)
    return 0;
  return inst->module()->pp_module();
}

void ResourceTracker::LastPluginRefWasDeleted(::ppapi::Resource* object) {
  ::ppapi::ResourceTracker::LastPluginRefWasDeleted(object);

  // TODO(brettw) this should be removed when we have the callback tracker
  // moved to the shared_impl. This will allow the logic to post aborts for
  // any callbacks directly in the Resource::LastPluginRefWasDeleted function
  // and we can remove this function altogether.
  PluginModule* plugin_module = ResourceHelper::GetPluginModule(object);
  if (plugin_module) {
    plugin_module->GetCallbackTracker()->PostAbortForResource(
        object->pp_resource());
  }
}

void ResourceTracker::AddNPObjectVar(NPObjectVar* object_var) {
  DCHECK(instance_map_.find(object_var->pp_instance()) != instance_map_.end());
  InstanceData& data = *instance_map_[object_var->pp_instance()].get();

  DCHECK(data.np_object_to_object_var.find(object_var->np_object()) ==
         data.np_object_to_object_var.end()) << "NPObjectVar already in map";
  data.np_object_to_object_var[object_var->np_object()] = object_var;
}

void ResourceTracker::RemoveNPObjectVar(NPObjectVar* object_var) {
  DCHECK(instance_map_.find(object_var->pp_instance()) != instance_map_.end());
  InstanceData& data = *instance_map_[object_var->pp_instance()].get();

  NPObjectToNPObjectVarMap::iterator found =
      data.np_object_to_object_var.find(object_var->np_object());
  if (found == data.np_object_to_object_var.end()) {
    NOTREACHED() << "NPObjectVar not registered.";
    return;
  }
  if (found->second != object_var) {
    NOTREACHED() << "NPObjectVar doesn't match.";
    return;
  }
  data.np_object_to_object_var.erase(found);
}

NPObjectVar* ResourceTracker::NPObjectVarForNPObject(PP_Instance instance,
                                                     NPObject* np_object) {
  DCHECK(instance_map_.find(instance) != instance_map_.end());
  InstanceData& data = *instance_map_[instance].get();

  NPObjectToNPObjectVarMap::iterator found =
      data.np_object_to_object_var.find(np_object);
  if (found == data.np_object_to_object_var.end())
    return NULL;
  return found->second;
}

int ResourceTracker::GetLiveNPObjectVarsForInstance(
    PP_Instance instance) const {
  InstanceMap::const_iterator found = instance_map_.find(instance);
  if (found == instance_map_.end())
    return 0;
  return static_cast<int>(found->second->np_object_to_object_var.size());
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
                               ::ppapi::PP_ID_TYPE_INSTANCE);
  } while (!new_instance ||
           instance_map_.find(new_instance) != instance_map_.end() ||
           !instance->module()->ReserveInstanceID(new_instance));

  instance_map_[new_instance] = linked_ptr<InstanceData>(new InstanceData);
  instance_map_[new_instance]->instance = instance;

  DidCreateInstance(new_instance);
  return new_instance;
}

void ResourceTracker::InstanceDeleted(PP_Instance instance) {
  DidDeleteInstance(instance);
  CleanupInstanceData(instance, true);
}

void ResourceTracker::InstanceCrashed(PP_Instance instance) {
  DidDeleteInstance(instance);
  CleanupInstanceData(instance, false);
}

PluginInstance* ResourceTracker::GetInstance(PP_Instance instance) {
  DLOG_IF(ERROR, !CheckIdType(instance, ::ppapi::PP_ID_TYPE_INSTANCE))
      << instance << " is not a PP_Instance.";
  InstanceMap::iterator found = instance_map_.find(instance);
  if (found == instance_map_.end())
    return NULL;
  return found->second->instance;
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
                             ::ppapi::PP_ID_TYPE_MODULE);
  } while (!new_module ||
           module_map_.find(new_module) != module_map_.end());
  module_map_[new_module] = module;
  return new_module;
}

void ResourceTracker::ModuleDeleted(PP_Module module) {
  DLOG_IF(ERROR, !CheckIdType(module, ::ppapi::PP_ID_TYPE_MODULE))
      << module << " is not a PP_Module.";
  ModuleMap::iterator found = module_map_.find(module);
  if (found == module_map_.end()) {
    NOTREACHED();
    return;
  }
  module_map_.erase(found);
}

PluginModule* ResourceTracker::GetModule(PP_Module module) {
  DLOG_IF(ERROR, !CheckIdType(module, ::ppapi::PP_ID_TYPE_MODULE))
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

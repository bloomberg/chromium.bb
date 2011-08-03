// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/plugin_var_tracker.h"

#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/interface_id.h"

namespace pp {
namespace proxy {

namespace {

// When non-NULL, this object overrides the VarTrackerSingleton.
PluginVarTracker* var_tracker_override = NULL;

}  // namespace

PluginVarTracker::HostVar::HostVar(PluginDispatcher* d, VarID i)
    : dispatcher(d),
      host_object_id(i) {
}

bool PluginVarTracker::HostVar::operator<(const HostVar& other) const {
  if (dispatcher < other.dispatcher)
    return true;
  if (other.dispatcher < dispatcher)
    return false;
  return host_object_id < other.host_object_id;
}

PluginVarTracker::PluginVarInfo::PluginVarInfo(const HostVar& host_var)
    : host_var(host_var),
      ref_count(0),
      track_with_no_reference_count(0) {
}

PluginVarTracker::PluginVarTracker() : last_plugin_var_id_(0) {
}

PluginVarTracker::~PluginVarTracker() {
}

// static
void PluginVarTracker::SetInstanceForTest(PluginVarTracker* tracker) {
  var_tracker_override = tracker;
}

// static
PluginVarTracker* PluginVarTracker::GetInstance() {
  if (var_tracker_override)
    return var_tracker_override;
  return Singleton<PluginVarTracker>::get();
}

PluginVarTracker::VarID PluginVarTracker::MakeString(const std::string& str) {
  return MakeString(str.c_str(), str.length());
}

PluginVarTracker::VarID PluginVarTracker::MakeString(const char* str,
                                                     uint32_t len) {
  std::pair<VarIDStringMap::iterator, bool>
      iter_success_pair(var_id_to_string_.end(), false);
  VarID new_id(0);
  RefCountedStringPtr str_ptr(new RefCountedString(str, len));
  // Pick new IDs until one is successfully inserted. This loop is very unlikely
  // to ever run a 2nd time, since we have ~2^63 possible IDs to exhaust.
  while (!iter_success_pair.second) {
    new_id = GetNewVarID();
    iter_success_pair =
        var_id_to_string_.insert(VarIDStringMap::value_type(new_id, str_ptr));
  }
  // Release the local pointer.
  str_ptr = NULL;
  // Now the map should have the only reference.
  DCHECK(iter_success_pair.first->second->HasOneRef());
  iter_success_pair.first->second->AddRef();
  return new_id;
}

const std::string* PluginVarTracker::GetExistingString(
    const PP_Var& var) const {
  if (var.type != PP_VARTYPE_STRING)
    return NULL;
  VarIDStringMap::const_iterator found =
      var_id_to_string_.find(var.value.as_id);
  if (found != var_id_to_string_.end())
    return &found->second->value();
  return NULL;
}

void PluginVarTracker::AddRef(const PP_Var& var) {
  if (var.type == PP_VARTYPE_STRING) {
    VarIDStringMap::iterator found = var_id_to_string_.find(var.value.as_id);
    if (found == var_id_to_string_.end()) {
      NOTREACHED() << "Requesting to addref an unknown string.";
      return;
    }
    found->second->AddRef();
  } else if (var.type == PP_VARTYPE_OBJECT && var.value.as_id != 0) {
    PluginVarInfoMap::iterator found = plugin_var_info_.find(var.value.as_id);
    if (found == plugin_var_info_.end()) {
      NOTREACHED() << "Requesting to addref an unknown object.";
      return;
    }

    PluginVarInfo& info = found->second;
    if (info.ref_count == 0) {
      // Got an AddRef for an object we have no existing reference for.
      // We need to tell the browser we've taken a ref. This comes up when the
      // browser passes an object as an input param and holds a ref for us.
      // This must be a sync message since otherwise the "addref" will actually
      // occur after the return to the browser of the sync function that
      // presumably sent the object.
      SendAddRefObjectMsg(info.host_var);
    }
    info.ref_count++;
  }
}

void PluginVarTracker::Release(const PP_Var& var) {
  if (var.type == PP_VARTYPE_STRING) {
    VarIDStringMap::iterator found = var_id_to_string_.find(var.value.as_id);
    if (found == var_id_to_string_.end()) {
      NOTREACHED() << "Requesting to release an unknown string.";
      return;
    }
    found->second->Release();
    // If there is only 1 reference left, it's the map's reference. Erase it
    // from the map, which will delete the string.
    if (found->second->HasOneRef())
      var_id_to_string_.erase(found);
  } else if (var.type == PP_VARTYPE_OBJECT) {
    PluginVarInfoMap::iterator found = plugin_var_info_.find(var.value.as_id);
    if (found == plugin_var_info_.end()) {
      NOTREACHED() << "Requesting to release an unknown object.";
      return;
    }

    PluginVarInfo& info = found->second;
    if (info.ref_count == 0) {
      NOTREACHED() << "Releasing an object with zero ref.";
      return;
    }

    info.ref_count--;
    if (info.ref_count == 0)
      SendReleaseObjectMsg(info.host_var);
    DeletePluginVarInfoIfNecessary(found);
  }
}

PP_Var PluginVarTracker::ReceiveObjectPassRef(const PP_Var& var,
                                              PluginDispatcher* dispatcher) {
  DCHECK(var.type == PP_VARTYPE_OBJECT);

  // Find the plugin info.
  PluginVarInfoMap::iterator found =
      FindOrMakePluginVarFromHostVar(var, dispatcher);
  if (found == plugin_var_info_.end()) {
    // The above code should have always made an entry in the map.
    NOTREACHED();
    return PP_MakeUndefined();
  }

  // Fix up the references. The host (renderer) just sent us a ref. The
  // renderer has addrefed the var in its tracker for us since it's returning
  // it.
  PluginVarInfo& info = found->second;
  if (info.ref_count == 0) {
    // We don't have a reference to this already, then we just add it to our
    // tracker and use that one ref.
    info.ref_count = 1;
  } else {
    // We already have a reference to it, that means the renderer now has two
    // references on our behalf. We want to transfer that extra reference to
    // our list. This means we addref in the plugin, and release the extra one
    // in the renderer.
    SendReleaseObjectMsg(info.host_var);
    info.ref_count++;
  }

  PP_Var ret;
  ret.type = PP_VARTYPE_OBJECT;
  ret.value.as_id = found->first;
  return ret;
}

PP_Var PluginVarTracker::TrackObjectWithNoReference(
    const PP_Var& host_var,
    PluginDispatcher* dispatcher) {
  DCHECK(host_var.type == PP_VARTYPE_OBJECT);

  PluginVarInfoMap::iterator found =
      FindOrMakePluginVarFromHostVar(host_var, dispatcher);
  if (found == plugin_var_info_.end()) {
    // The above code should have always made an entry in the map.
    NOTREACHED();
    return PP_MakeUndefined();
  }

  found->second.track_with_no_reference_count++;

  PP_Var ret;
  ret.type = PP_VARTYPE_OBJECT;
  ret.value.as_id = found->first;
  return ret;
}

void PluginVarTracker::StopTrackingObjectWithNoReference(
    const PP_Var& plugin_var) {
  DCHECK(plugin_var.type == PP_VARTYPE_OBJECT);
  PluginVarInfoMap::iterator found = plugin_var_info_.find(
      plugin_var.value.as_id);
  if (found == plugin_var_info_.end()) {
    NOTREACHED();
    return;
  }

  found->second.track_with_no_reference_count--;
  DeletePluginVarInfoIfNecessary(found);
}

PP_Var PluginVarTracker::GetHostObject(const PP_Var& plugin_object) const {
  DCHECK(plugin_object.type == PP_VARTYPE_OBJECT);
  PluginVarInfoMap::const_iterator found = plugin_var_info_.find(
      plugin_object.value.as_id);
  if (found == plugin_var_info_.end()) {
    NOTREACHED();
    return PP_MakeUndefined();
  }
  PP_Var ret;
  ret.type = PP_VARTYPE_OBJECT;
  ret.value.as_id = found->second.host_var.host_object_id;
  return ret;
}

PluginDispatcher* PluginVarTracker::DispatcherForPluginObject(
    const PP_Var& plugin_object) const {
  DCHECK(plugin_object.type == PP_VARTYPE_OBJECT);
  PluginVarInfoMap::const_iterator found = plugin_var_info_.find(
      plugin_object.value.as_id);
  if (found != plugin_var_info_.end())
    return found->second.host_var.dispatcher;
  return NULL;
}

void PluginVarTracker::ReleaseHostObject(PluginDispatcher* dispatcher,
                                         const PP_Var& host_object) {
  // Convert the host object to a normal var valid in the plugin.
  DCHECK(host_object.type == PP_VARTYPE_OBJECT);
  HostVarToPluginVarMap::iterator found = host_var_to_plugin_var_.find(
      HostVar(dispatcher, host_object.value.as_id));
  if (found == host_var_to_plugin_var_.end()) {
    NOTREACHED();
    return;
  }

  // Now just release the object like normal.
  PP_Var plugin_object;
  plugin_object.type = PP_VARTYPE_OBJECT;
  plugin_object.value.as_id = found->second;
  Release(plugin_object);
}

int PluginVarTracker::GetRefCountForObject(const PP_Var& plugin_object) {
  PluginVarInfoMap::iterator found = plugin_var_info_.find(
      plugin_object.value.as_id);
  if (found == plugin_var_info_.end())
    return -1;
  return found->second.ref_count;
}

int PluginVarTracker::GetTrackedWithNoReferenceCountForObject(
    const PP_Var& plugin_object) {
  PluginVarInfoMap::iterator found = plugin_var_info_.find(
      plugin_object.value.as_id);
  if (found == plugin_var_info_.end())
    return -1;
  return found->second.track_with_no_reference_count;
}

void PluginVarTracker::SendAddRefObjectMsg(const HostVar& host_var) {
  int unused;
  host_var.dispatcher->Send(new PpapiHostMsg_PPBVar_AddRefObject(
      INTERFACE_ID_PPB_VAR_DEPRECATED, host_var.host_object_id, &unused));
}

void PluginVarTracker::SendReleaseObjectMsg(const HostVar& host_var) {
  host_var.dispatcher->Send(new PpapiHostMsg_PPBVar_ReleaseObject(
      INTERFACE_ID_PPB_VAR_DEPRECATED, host_var.host_object_id));
}

PluginVarTracker::PluginVarInfoMap::iterator
PluginVarTracker::FindOrMakePluginVarFromHostVar(const PP_Var& var,
                                                 PluginDispatcher* dispatcher) {
  DCHECK(var.type == PP_VARTYPE_OBJECT);
  HostVar host_var(dispatcher, var.value.as_id);

  HostVarToPluginVarMap::iterator found =
      host_var_to_plugin_var_.find(host_var);
  if (found != host_var_to_plugin_var_.end()) {
    PluginVarInfoMap::iterator ret = plugin_var_info_.find(found->second);
    DCHECK(ret != plugin_var_info_.end());
    return ret;  // Already know about this var return the ID.
  }

  // Make a new var, adding references to both maps.
  VarID new_plugin_var_id = GetNewVarID();
  host_var_to_plugin_var_[host_var] = new_plugin_var_id;
  return plugin_var_info_.insert(
      std::make_pair(new_plugin_var_id, PluginVarInfo(host_var))).first;
}

void PluginVarTracker::DeletePluginVarInfoIfNecessary(
    PluginVarInfoMap::iterator iter) {
  if (iter->second.ref_count != 0 ||
      iter->second.track_with_no_reference_count != 0)
    return;  // Object still alive.

  // Object ref counts are all zero, delete from both maps.
  DCHECK(host_var_to_plugin_var_.find(iter->second.host_var) !=
         host_var_to_plugin_var_.end());
  host_var_to_plugin_var_.erase(iter->second.host_var);
  plugin_var_info_.erase(iter);
}

PluginVarTracker::VarID PluginVarTracker::GetNewVarID() {
  if (last_plugin_var_id_ == std::numeric_limits<VarID>::max())
    last_plugin_var_id_ = 0;
  return ++last_plugin_var_id_;
}

}  // namesace proxy
}  // namespace pp

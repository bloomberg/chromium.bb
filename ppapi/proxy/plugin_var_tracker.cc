// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/plugin_var_tracker.h"

#include "base/ref_counted.h"
#include "base/singleton.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/interface_id.h"

namespace pp {
namespace proxy {

namespace {

// When non-NULL, this object overrides the VarTrackerSingleton.
PluginVarTracker* var_tracker_override = NULL;

class RefCountedString : public base::RefCounted<RefCountedString> {
 public:
  RefCountedString() {
  }
  RefCountedString(const std::string& str) : value_(str) {
  }
  RefCountedString(const char* data, size_t len)
      : value_(data, len) {
  }

  const std::string& value() const { return value_; }

 private:
  std::string value_;
};

// When running in the plugin, this will convert the string ID to the object
// using casting. No validity checking is done.
RefCountedString* PluginStringFromID(PluginVarTracker::VarID id) {
  return reinterpret_cast<RefCountedString*>(static_cast<intptr_t>(id));
}

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

PluginVarTracker::PluginVarTracker() : last_plugin_object_id_(0) {
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
  RefCountedString* out = new RefCountedString(str);
  out->AddRef();
  return static_cast<VarID>(reinterpret_cast<intptr_t>(out));
}

PluginVarTracker::VarID PluginVarTracker::MakeString(const char* str,
                                                     uint32_t len) {
  RefCountedString* out = new RefCountedString(str, len);
  out->AddRef();
  return static_cast<VarID>(reinterpret_cast<intptr_t>(out));
}

std::string PluginVarTracker::GetString(const PP_Var& var) const {
  return PluginStringFromID(var.value.as_id)->value();
}

const std::string* PluginVarTracker::GetExistingString(
    const PP_Var& var) const {
  if (var.type != PP_VARTYPE_STRING)
    return NULL;
  RefCountedString* str = PluginStringFromID(var.value.as_id);
  return &str->value();
}

void PluginVarTracker::AddRef(const PP_Var& var) {
  if (var.type == PP_VARTYPE_STRING) {
    PluginStringFromID(var.value.as_id)->AddRef();
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
      SendAddRefObjectMsg(info.host_var);
    }
    info.ref_count++;
  }
}

void PluginVarTracker::Release(const PP_Var& var) {
  if (var.type == PP_VARTYPE_STRING) {
    PluginStringFromID(var.value.as_id)->Release();
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
  host_var.dispatcher->Send(new PpapiHostMsg_PPBVar_AddRefObject(
      INTERFACE_ID_PPB_VAR_DEPRECATED, host_var.host_object_id));
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
  VarID new_plugin_var_id = ++last_plugin_object_id_;
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

}  // namesace proxy
}  // namespace pp

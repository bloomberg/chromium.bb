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
#include "ppapi/proxy/proxy_object_var.h"
#include "ppapi/shared_impl/var.h"

using ppapi::ProxyObjectVar;
using ppapi::Var;

namespace pp {
namespace proxy {

PluginVarTracker::HostVar::HostVar(PluginDispatcher* d, int32 i)
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

PluginVarTracker::PluginVarTracker() {
}

PluginVarTracker::~PluginVarTracker() {
}

PP_Var PluginVarTracker::ReceiveObjectPassRef(const PP_Var& host_var,
                                              PluginDispatcher* dispatcher) {
  DCHECK(host_var.type == PP_VARTYPE_OBJECT);

  // Get the object.
  scoped_refptr<ProxyObjectVar> object(
      FindOrMakePluginVarFromHostVar(host_var, dispatcher));

  // Actually create the PP_Var, this will add all the tracking info but not
  // adjust any refcounts.
  PP_Var ret = GetOrCreateObjectVarID(object.get());

  VarInfo& info = GetLiveVar(ret)->second;
  if (info.ref_count > 0) {
    // We already had a reference to it before. That means the renderer now has
    // two references on our behalf. We want to transfer that extra reference
    // to our list. This means we addref in the plugin, and release the extra
    // one in the renderer.
    SendReleaseObjectMsg(*object);
  }
  info.ref_count++;
  return ret;
}

PP_Var PluginVarTracker::TrackObjectWithNoReference(
    const PP_Var& host_var,
    PluginDispatcher* dispatcher) {
  DCHECK(host_var.type == PP_VARTYPE_OBJECT);

  // Get the object.
  scoped_refptr<ProxyObjectVar> object(
      FindOrMakePluginVarFromHostVar(host_var, dispatcher));

  // Actually create the PP_Var, this will add all the tracking info but not
  // adjust any refcounts.
  PP_Var ret = GetOrCreateObjectVarID(object.get());

  VarInfo& info = GetLiveVar(ret)->second;
  info.track_with_no_reference_count++;
  return ret;
}

void PluginVarTracker::StopTrackingObjectWithNoReference(
    const PP_Var& plugin_var) {
  DCHECK(plugin_var.type == PP_VARTYPE_OBJECT);
  VarMap::iterator found = GetLiveVar(plugin_var);
  if (found == live_vars_.end()) {
    NOTREACHED();
    return;
  }

  DCHECK(found->second.track_with_no_reference_count > 0);
  found->second.track_with_no_reference_count--;
  DeleteObjectInfoIfNecessary(found);
}

PP_Var PluginVarTracker::GetHostObject(const PP_Var& plugin_object) const {
  if (plugin_object.type != PP_VARTYPE_OBJECT) {
    NOTREACHED();
    return PP_MakeUndefined();
  }

  Var* var = GetVar(plugin_object);
  ProxyObjectVar* object = var->AsProxyObjectVar();
  if (!object) {
    NOTREACHED();
    return PP_MakeUndefined();
  }

  // Make a var with the host ID.
  PP_Var ret;
  ret.type = PP_VARTYPE_OBJECT;
  ret.value.as_id = object->host_var_id();
  return ret;
}

PluginDispatcher* PluginVarTracker::DispatcherForPluginObject(
    const PP_Var& plugin_object) const {
  if (plugin_object.type != PP_VARTYPE_OBJECT)
    return NULL;

  VarMap::const_iterator found = GetLiveVar(plugin_object);
  if (found == live_vars_.end())
    return NULL;

  ProxyObjectVar* object = found->second.var->AsProxyObjectVar();
  if (!object)
    return NULL;
  return object->dispatcher();
}

void PluginVarTracker::ReleaseHostObject(PluginDispatcher* dispatcher,
                                         const PP_Var& host_object) {
  // Convert the host object to a normal var valid in the plugin.
  DCHECK(host_object.type == PP_VARTYPE_OBJECT);
  HostVarToPluginVarMap::iterator found = host_var_to_plugin_var_.find(
      HostVar(dispatcher, static_cast<int32>(host_object.value.as_id)));
  if (found == host_var_to_plugin_var_.end()) {
    NOTREACHED();
    return;
  }

  // Now just release the object given the plugin var ID.
  ReleaseVar(found->second);
}

int PluginVarTracker::GetRefCountForObject(const PP_Var& plugin_object) {
  VarMap::iterator found = GetLiveVar(plugin_object);
  if (found == live_vars_.end())
    return -1;
  return found->second.ref_count;
}

int PluginVarTracker::GetTrackedWithNoReferenceCountForObject(
    const PP_Var& plugin_object) {
  VarMap::iterator found = GetLiveVar(plugin_object);
  if (found == live_vars_.end())
    return -1;
  return found->second.track_with_no_reference_count;
}

int32 PluginVarTracker::AddVarInternal(Var* var, AddVarRefMode mode) {
  // Normal adding.
  int32 new_id = VarTracker::AddVarInternal(var, mode);

  // Need to add proxy objects to the host var map.
  ProxyObjectVar* proxy_object = var->AsProxyObjectVar();
  if (proxy_object) {
    HostVar host_var(proxy_object->dispatcher(), proxy_object->host_var_id());
    DCHECK(host_var_to_plugin_var_.find(host_var) ==
           host_var_to_plugin_var_.end());  // Adding an object twice, use
                                            // FindOrMakePluginVarFromHostVar.
    host_var_to_plugin_var_[host_var] = new_id;
  }
  return new_id;
}

void PluginVarTracker::TrackedObjectGettingOneRef(VarMap::const_iterator iter) {
  ProxyObjectVar* object = iter->second.var->AsProxyObjectVar();
  if (!object) {
    NOTREACHED();
    return;
  }

  DCHECK(iter->second.ref_count == 0);

  // Got an AddRef for an object we have no existing reference for.
  // We need to tell the browser we've taken a ref. This comes up when the
  // browser passes an object as an input param and holds a ref for us.
  // This must be a sync message since otherwise the "addref" will actually
  // occur after the return to the browser of the sync function that
  // presumably sent the object.
  SendAddRefObjectMsg(*object);
}

void PluginVarTracker::ObjectGettingZeroRef(VarMap::iterator iter) {
  ProxyObjectVar* object = iter->second.var->AsProxyObjectVar();
  if (!object) {
    NOTREACHED();
    return;
  }

  // Notify the host we're no longer holding our ref.
  DCHECK(iter->second.ref_count == 0);
  SendReleaseObjectMsg(*object);

  // This will optionally delete the info from live_vars_.
  VarTracker::ObjectGettingZeroRef(iter);
}

bool PluginVarTracker::DeleteObjectInfoIfNecessary(VarMap::iterator iter) {
  // Get the info before calling the base class's version of this function,
  // which may delete the object.
  ProxyObjectVar* object = iter->second.var->AsProxyObjectVar();
  HostVar host_var(object->dispatcher(), object->host_var_id());

  if (!VarTracker::DeleteObjectInfoIfNecessary(iter))
    return false;

  // Clean up the host var mapping.
  DCHECK(host_var_to_plugin_var_.find(host_var) !=
         host_var_to_plugin_var_.end());
  host_var_to_plugin_var_.erase(host_var);
  return true;
}

PP_Var PluginVarTracker::GetOrCreateObjectVarID(ProxyObjectVar* object) {
  // We can't use object->GetPPVar() because we don't want to affect the
  // refcount, so we have to add everything manually here.
  int32 var_id = object->GetExistingVarID();
  if (!var_id) {
    var_id = AddVarInternal(object, ADD_VAR_CREATE_WITH_NO_REFERENCE);
    object->AssignVarID(var_id);
  }

  PP_Var ret;
  ret.type = PP_VARTYPE_OBJECT;
  ret.value.as_id = var_id;
  return ret;
}

void PluginVarTracker::SendAddRefObjectMsg(
    const ProxyObjectVar& proxy_object) {
  int unused;
  proxy_object.dispatcher()->Send(new PpapiHostMsg_PPBVar_AddRefObject(
      INTERFACE_ID_PPB_VAR_DEPRECATED, proxy_object.host_var_id(), &unused));
}

void PluginVarTracker::SendReleaseObjectMsg(
    const ProxyObjectVar& proxy_object) {
  proxy_object.dispatcher()->Send(new PpapiHostMsg_PPBVar_ReleaseObject(
      INTERFACE_ID_PPB_VAR_DEPRECATED, proxy_object.host_var_id()));
}

scoped_refptr<ProxyObjectVar> PluginVarTracker::FindOrMakePluginVarFromHostVar(
    const PP_Var& var,
    PluginDispatcher* dispatcher) {
  DCHECK(var.type == PP_VARTYPE_OBJECT);
  HostVar host_var(dispatcher, var.value.as_id);

  HostVarToPluginVarMap::iterator found =
      host_var_to_plugin_var_.find(host_var);
  if (found == host_var_to_plugin_var_.end()) {
    // Create a new object.
    return scoped_refptr<ProxyObjectVar>(
        new ProxyObjectVar(dispatcher, static_cast<int32>(var.value.as_id)));
  }

  // Have this host var, look up the object.
  VarMap::iterator ret = live_vars_.find(found->second);
  DCHECK(ret != live_vars_.end());

  // All objects should be proxy objects.
  DCHECK(ret->second.var->AsProxyObjectVar());
  return scoped_refptr<ProxyObjectVar>(ret->second.var->AsProxyObjectVar());
}

}  // namesace proxy
}  // namespace pp

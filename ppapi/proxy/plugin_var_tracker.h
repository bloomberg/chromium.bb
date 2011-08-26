// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PLUGIN_VAR_TRACKER_H_
#define PPAPI_PROXY_PLUGIN_VAR_TRACKER_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/shared_impl/var_tracker.h"

struct PPB_Var;

template<typename T> struct DefaultSingletonTraits;

namespace ppapi {

class ProxyObjectVar;

namespace proxy {

class PluginDispatcher;

// Tracks live strings and objects in the plugin process.
class PPAPI_PROXY_EXPORT PluginVarTracker : public VarTracker {
 public:
  PluginVarTracker();
  ~PluginVarTracker();

  // Manages tracking for receiving a VARTYPE_OBJECT from the remote side
  // (either the plugin or the renderer) that has already had its reference
  // count incremented on behalf of the caller.
  PP_Var ReceiveObjectPassRef(const PP_Var& var, PluginDispatcher* dispatcher);

  // See the comment in var_tracker.h for more about what a tracked object is.
  // This adds and releases the "track_with_no_reference_count" for a given
  // object.
  PP_Var TrackObjectWithNoReference(const PP_Var& host_var,
                                    PluginDispatcher* dispatcher);
  void StopTrackingObjectWithNoReference(const PP_Var& plugin_var);

  // Returns the host var for the corresponding plugin object var. The object
  // should be a VARTYPE_OBJECT. The reference count is not affeceted.
  PP_Var GetHostObject(const PP_Var& plugin_object) const;

  PluginDispatcher* DispatcherForPluginObject(
      const PP_Var& plugin_object) const;

  // Like Release() but the var is identified by its host object ID (as
  // returned by GetHostObject).
  void ReleaseHostObject(PluginDispatcher* dispatcher,
                         const PP_Var& host_object);

  // Retrieves the internal reference counts for testing. Returns 0 if we
  // know about the object but the corresponding value is 0, or -1 if the
  // given object ID isn't in our map.
  int GetRefCountForObject(const PP_Var& plugin_object);
  int GetTrackedWithNoReferenceCountForObject(const PP_Var& plugin_object);

 protected:
  // VarTracker protected overrides.
  virtual int32 AddVarInternal(Var* var, AddVarRefMode mode) OVERRIDE;
  virtual void TrackedObjectGettingOneRef(VarMap::const_iterator iter) OVERRIDE;
  virtual void ObjectGettingZeroRef(VarMap::iterator iter) OVERRIDE;
  virtual bool DeleteObjectInfoIfNecessary(VarMap::iterator iter) OVERRIDE;

 private:
  friend struct DefaultSingletonTraits<PluginVarTracker>;
  friend class PluginProxyTestHarness;

  // Represents a var as received from the host.
  struct HostVar {
    HostVar(PluginDispatcher* d, int32 i);

    bool operator<(const HostVar& other) const;

    // The dispatcher that sent us this object. This is used so we know how to
    // send back requests on this object.
    PluginDispatcher* dispatcher;

    // The object ID that the host generated to identify the object. This is
    // unique only within that host: different hosts could give us different
    // objects with the same ID.
    int32 host_object_id;
  };

  // Returns the existing var ID for the given object var, creating and
  // assigning an ID to it if necessary. This does not affect the reference
  // count, so in the creation case the refcount will be 0. It's assumed in
  // this case the caller will either adjust the refcount or the
  // track_with_no_reference_count.
  PP_Var GetOrCreateObjectVarID(ProxyObjectVar* object);

  // Sends an addref or release message to the browser for the given object ID.
  void SendAddRefObjectMsg(const ProxyObjectVar& proxy_object);
  void SendReleaseObjectMsg(const ProxyObjectVar& proxy_object);

  // Looks up the given host var. If we already know about it, returns a
  // reference to the already-tracked object. If it doesn't creates a new one
  // and returns it. If it's created, it's not added to the map.
  scoped_refptr<ProxyObjectVar> FindOrMakePluginVarFromHostVar(
      const PP_Var& var,
      PluginDispatcher* dispatcher);

  // Maps host vars in the host to IDs in the plugin process.
  typedef std::map<HostVar, int32> HostVarToPluginVarMap;
  HostVarToPluginVarMap host_var_to_plugin_var_;

  DISALLOW_COPY_AND_ASSIGN(PluginVarTracker);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PLUGIN_VAR_TRACKER_H_

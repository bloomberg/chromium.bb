// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PLUGIN_VAR_TRACKER_H_
#define PPAPI_PROXY_PLUGIN_VAR_TRACKER_H_

#include <map>
#include <string>

#include "ipc/ipc_channel.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"

struct PPB_Var;

template<typename T> struct DefaultSingletonTraits;

namespace pp {
namespace proxy {

// Tracks live strings and objects in the plugin process.
//
// This object maintains its own object IDs that are used by the plugin. These
// IDs can be mapped to the renderer that created them, and that renderer's ID.
// This way, we can maintain multiple renderers each giving us objects, and the
// plugin can work with them using a uniform set of unique IDs.
//
// We maintain our own reference count for objects.  a single ref in the
// renderer process whenever we have a nonzero refcount in the plugin process.
// This allows AddRef and Release to not initiate too much IPC chat.
//
// In addition to the local reference count, we also maintain "tracked objects"
// which are objects that the plugin is aware of, but doesn't hold a reference
// to. This will happen when the plugin is passed an object as an argument from
// the host (renderer) but where a reference is not passed.
class PluginVarTracker {
 public:
  typedef int64_t VarID;

  // This uses the PluginDispatcher to identify the source of vars so that
  // the proper messages can be sent back. However, since all we need is the
  // ability to send messages, we can always use the Sender base class of
  // Dispatcher in this class, which makes it easy to unit test.
  typedef IPC::Channel::Sender Sender;

  // Called by tests that want to specify a specific VarTracker. This allows
  // them to use a unique one each time and avoids singletons sticking around
  // across tests.
  static void SetInstanceForTest(PluginVarTracker* tracker);

  // Returns the global var tracker for the plugin object.
  static PluginVarTracker* GetInstance();

  // Allocates a string and returns the ID of it. The refcount will be 1.
  VarID MakeString(const std::string& str);
  VarID MakeString(const char* str, uint32_t len);

  // Returns the string associated with the given string var. The var must be
  // of type string and must be valid or this function will crash.
  std::string GetString(const PP_Var& plugin_var) const;

  // Returns a pointer to the given string if it exists, or NULL if the var
  // isn't a string var.
  const std::string* GetExistingString(const PP_Var& plugin_var) const;

  void AddRef(const PP_Var& plugin_var);
  void Release(const PP_Var& plugin_var);

  // Manages tracking for receiving a VARTYPE_OBJECT from the remote side
  // (either the plugin or the renderer) that has already had its reference
  // count incremented on behalf of the caller.
  PP_Var ReceiveObjectPassRef(const PP_Var& var, Sender* channel);

  PP_Var TrackObjectWithNoReference(const PP_Var& host_var,
                                    Sender* channel);
  void StopTrackingObjectWithNoReference(const PP_Var& plugin_var);

  // Returns the host var for the corresponding plugin object var. The object
  // should be a VARTYPE_OBJECT
  PP_Var GetHostObject(const PP_Var& plugin_object) const;

  // Like Release() but the var is identified by its host object ID (as
  // returned by GetHostObject).
  void ReleaseHostObject(Sender* sender, const PP_Var& host_object);

  // Retrieves the internal reference counts for testing. Returns 0 if we
  // know about the object but the corresponding value is 0, or -1 if the
  // given object ID isn't in our map.
  int GetRefCountForObject(const PP_Var& plugin_object);
  int GetTrackedWithNoReferenceCountForObject(const PP_Var& plugin_object);

 private:
  friend struct DefaultSingletonTraits<PluginVarTracker>;
  friend class PluginProxyTest;

  // Represents a var as received from the host.
  struct HostVar {
    HostVar(Sender* s, int64_t i);

    bool operator<(const HostVar& other) const;

    // The host that sent us this object. This is used so we know how to send
    // back requests on this object.
    Sender* channel;

    // The object ID that the host generated to identify the object. This is
    // unique only within that host: different hosts could give us different
    // objects with the same ID.
    VarID host_object_id;
  };

  // The information associated with a var object in the plugin.
  struct PluginVarInfo {
    PluginVarInfo(const HostVar& host_var);

    // Maps back to the original var in the host.
    HostVar host_var;

    // Explicit reference count. This value is affected by the renderer calling
    // AddRef and Release. A nonzero value here is represented by a single
    // reference in the host on our behalf (this reduces IPC traffic).
    int32_t ref_count;

    // Tracked object count (see class comment above).
    //
    // "TrackObjectWithNoReference" might be called recursively in rare cases.
    // For example, say the host calls a plugin function with an object as an
    // argument, and in response, the plugin calls a host function that then
    // calls another (or the same) plugin function with the same object.
    //
    // This value tracks the number of calls to TrackObjectWithNoReference so
    // we know when we can stop tracking this object.
    int32_t track_with_no_reference_count;
  };

  typedef std::map<int64_t, PluginVarInfo> PluginVarInfoMap;

  PluginVarTracker();
  ~PluginVarTracker();

  // Sends an addref or release message to the browser for the given object ID.
  void SendAddRefObjectMsg(const HostVar& host_var);
  void SendReleaseObjectMsg(const HostVar& host_var);

  PluginVarInfoMap::iterator FindOrMakePluginVarFromHostVar(
      const PP_Var& var,
      Sender* channel);

  // Checks the reference counds of the given plugin var info and removes the
  // tracking information if necessary. We're done with the object when its
  // explicit reference count and its "tracked with no reference" count both
  // reach zero.
  void DeletePluginVarInfoIfNecessary(PluginVarInfoMap::iterator iter);

  // Tracks all information about plugin vars.
  PluginVarInfoMap plugin_var_info_;

  // Maps host vars to plugin vars. This allows us to know if we've previously
  // seen a host var and re-use the information.
  typedef std::map<HostVar, VarID> HostVarToPluginVarMap;
  HostVarToPluginVarMap host_var_to_plugin_var_;

  // The last plugin object ID we've handed out. This must be unique for the
  // process.
  VarID last_plugin_object_id_;
};

}  // namespace proxy
}  // namespace pp

#endif  // PPAPI_PROXY_PLUGIN_VAR_TRACKER_H_

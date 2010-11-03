// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PLUGIN_VAR_TRACKER_H_
#define PPAPI_PROXY_PLUGIN_VAR_TRACKER_H_

#include <map>
#include <string>

#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_var.h"

struct PPB_Var;

namespace pp {
namespace proxy {

class PluginDispatcher;

// Tracks live strings and objects in the plugin process. We maintain our own
// reference count for these objects. In the case of JS objects, we maintain
// a single ref in the renderer process whenever we have a nonzero refcount
// in the plugin process. This allows AddRef and Release to not initiate too
// much IPC chat.
class PluginVarTracker {
 public:
  // You must call Init() after creation to set up the correct interfaces. We
  // do this to avoid having to depend on the dispatcher in the constructor,
  // which is probably just being created from our constructor.
  PluginVarTracker(PluginDispatcher* dispatcher);

  // Must be called after construction.
  void Init();

  // Allocates a string and returns the ID of it. The refcount will be 1.
  int64_t MakeString(const std::string& str);

  // Returns the string associated with the given string var. The var must be
  // of type string and must be valid or this function will crash.
  std::string GetString(const PP_Var& var) const;

  // Returns a pointer to the given string if it exists, or NULL if the var
  // isn't a string var.
  const std::string* GetExistingString(const PP_Var& var) const;

  void AddRef(const PP_Var& var);
  void Release(const PP_Var& var);

  // Manages tracking for receiving a VARTYPE_OBJECT from the remote side
  // (either the plugin or the renderer) that has already had its reference
  // count incremented on behalf of the caller.
  void ReceiveObjectPassRef(const PP_Var& var);

 private:
  // Sends an addref or release message to the browser for the given object ID.
  void SendAddRefObjectMsg(int64_t id);
  void SendReleaseObjectMsg(int64_t id);

  PluginDispatcher* dispatcher_;

  // Tracks object references to the reference count of that object on the
  // plugin side.
  typedef std::map<int64_t, int> ObjectRefCount;
  ObjectRefCount object_ref_count_;
};

}  // namespace proxy
}  // namespace pp

#endif  // PPAPI_PROXY_PLUGIN_VAR_TRACKER_H_

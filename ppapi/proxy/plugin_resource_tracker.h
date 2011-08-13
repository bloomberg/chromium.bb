// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PLUGIN_RESOURCE_TRACKER_H_
#define PPAPI_PROXY_PLUGIN_RESOURCE_TRACKER_H_

#include <map>
#include <utility>

#include "base/compiler_specific.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/proxy/plugin_var_tracker.h"
#include "ppapi/shared_impl/host_resource.h"
#include "ppapi/shared_impl/tracker_base.h"

template<typename T> struct DefaultSingletonTraits;

namespace ppapi {
class Var;
}

namespace pp {
namespace proxy {

class PluginDispatcher;
class PluginResource;

class PluginResourceTracker : public ::ppapi::TrackerBase {
 public:
  // Called by tests that want to specify a specific ResourceTracker. This
  // allows them to use a unique one each time and avoids singletons sticking
  // around across tests.
  static void SetInstanceForTest(PluginResourceTracker* tracker);

  // Returns the global singleton resource tracker for the plugin.
  static PluginResourceTracker* GetInstance();
  static ::ppapi::TrackerBase* GetTrackerBaseInstance();

  // Returns the object associated with the given resource ID, or NULL if
  // there isn't one.
  PluginResource* GetResourceObject(PP_Resource pp_resource);

  // Adds the given resource object to the tracked list, and returns the
  // plugin-local PP_Resource ID that identifies the resource. Note that this
  // PP_Resource is not valid to send to the host, use
  // PluginResource.host_resource() to get that.
  //
  // The resource tracker will take a reference to the given object.
  PP_Resource AddResource(PluginResource* object);

  void AddRefResource(PP_Resource resource);
  void ReleaseResource(PP_Resource resource);

  // Given a host resource, maps it to an existing plugin resource ID if it
  // exists, or returns 0 on failure.
  PP_Resource PluginResourceForHostResource(
      const ppapi::HostResource& resource) const;

  PluginVarTracker& var_tracker() {
    return var_tracker_test_override_ ? *var_tracker_test_override_
                                      : var_tracker_;
  }

  void set_var_tracker_test_override(PluginVarTracker* t) {
    var_tracker_test_override_ = t;
  }

  // TrackerBase.
  virtual ppapi::ResourceObjectBase* GetResourceAPI(
      PP_Resource res) OVERRIDE;
  virtual ppapi::FunctionGroupBase* GetFunctionAPI(
      PP_Instance inst,
      pp::proxy::InterfaceID id) OVERRIDE;
  virtual PP_Instance GetInstanceForResource(PP_Resource resource) OVERRIDE;
  virtual ppapi::VarTracker* GetVarTracker() OVERRIDE;

 private:
  friend struct DefaultSingletonTraits<PluginResourceTracker>;
  friend class PluginResourceTrackerTest;
  friend class PluginProxyTestHarness;

  PluginResourceTracker();
  virtual ~PluginResourceTracker();

  struct ResourceInfo {
    ResourceInfo();
    ResourceInfo(int ref_count, PluginResource* r);
    ResourceInfo(const ResourceInfo& other);
    ~ResourceInfo();

    ResourceInfo& operator=(const ResourceInfo& other);

    int ref_count;
    scoped_refptr<PluginResource> resource;  // May be NULL.
  };

  void ReleasePluginResourceRef(const PP_Resource& var,
                                bool notify_browser_on_release);

  // Use the var_tracker_test_override_ instead if it's non-NULL.
  //
  // TODO(brettw) this should be somehow separated out from here. I'm thinking
  // of some global object that manages PPAPI globals, including separate var
  // and resource trackers.
  PluginVarTracker var_tracker_;

  // Non-owning pointer to a var tracker mock used by tests. NULL when no
  // test implementation is provided.
  PluginVarTracker* var_tracker_test_override_;

  // Map of plugin resource IDs to the information tracking that resource.
  typedef std::map<PP_Resource, ResourceInfo> ResourceMap;
  ResourceMap resource_map_;

  // Map of host instance/resource pairs to a plugin resource ID.
  typedef std::map<ppapi::HostResource, PP_Resource> HostResourceMap;
  HostResourceMap host_resource_map_;

  // Tracks the last ID we've sent out as a plugin resource so we don't send
  // duplicates.
  PP_Resource last_resource_id_;

  DISALLOW_COPY_AND_ASSIGN(PluginResourceTracker);
};

}  // namespace proxy
}  // namespace pp

#endif  // PPAPI_PROXY_PLUGIN_RESOURCE_TRACKER_H_

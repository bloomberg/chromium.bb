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
#include "ppapi/shared_impl/resource_tracker.h"
#include "ppapi/shared_impl/tracker_base.h"

template<typename T> struct DefaultSingletonTraits;

namespace ppapi {

class Var;

namespace proxy {

class PluginDispatcher;

class PluginResourceTracker : public TrackerBase, public ResourceTracker {
 public:
  // Called by tests that want to specify a specific ResourceTracker. This
  // allows them to use a unique one each time and avoids singletons sticking
  // around across tests.
  static void SetInstanceForTest(PluginResourceTracker* tracker);

  // Returns the global singleton resource tracker for the plugin.
  static PluginResourceTracker* GetInstance();
  static TrackerBase* GetTrackerBaseInstance();

  // Given a host resource, maps it to an existing plugin resource ID if it
  // exists, or returns 0 on failure.
  PP_Resource PluginResourceForHostResource(
      const HostResource& resource) const;

  PluginVarTracker& var_tracker() {
    return var_tracker_test_override_ ? *var_tracker_test_override_
                                      : var_tracker_;
  }

  void set_var_tracker_test_override(PluginVarTracker* t) {
    var_tracker_test_override_ = t;
  }

  // TrackerBase.
  virtual FunctionGroupBase* GetFunctionAPI(PP_Instance inst,
                                            InterfaceID id) OVERRIDE;
  virtual VarTracker* GetVarTracker() OVERRIDE;
  virtual ResourceTracker* GetResourceTracker() OVERRIDE;
  virtual PP_Module GetModuleForInstance(PP_Instance instance) OVERRIDE;

 protected:
  // ResourceTracker overrides.
  virtual PP_Resource AddResource(Resource* object) OVERRIDE;
  virtual void RemoveResource(Resource* object) OVERRIDE;

 private:
  friend struct DefaultSingletonTraits<PluginResourceTracker>;
  friend class PluginResourceTrackerTest;
  friend class PluginProxyTestHarness;

  PluginResourceTracker();
  virtual ~PluginResourceTracker();

  // Use the var_tracker_test_override_ instead if it's non-NULL.
  //
  // TODO(brettw) this should be somehow separated out from here. I'm thinking
  // of some global object that manages PPAPI globals, including separate var
  // and resource trackers.
  PluginVarTracker var_tracker_;

  // Non-owning pointer to a var tracker mock used by tests. NULL when no
  // test implementation is provided.
  PluginVarTracker* var_tracker_test_override_;

  // Map of host instance/resource pairs to a plugin resource ID.
  typedef std::map<HostResource, PP_Resource> HostResourceMap;
  HostResourceMap host_resource_map_;

  DISALLOW_COPY_AND_ASSIGN(PluginResourceTracker);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PLUGIN_RESOURCE_TRACKER_H_

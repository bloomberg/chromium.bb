// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PLUGIN_RESOURCE_TRACKER_H_
#define PPAPI_PROXY_PLUGIN_RESOURCE_TRACKER_H_

#include <map>

#include "base/linked_ptr.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_var.h"

template<typename T> struct DefaultSingletonTraits;

namespace pp {
namespace proxy {

class PluginDispatcher;
class PluginResource;

class PluginResourceTracker {
 public:
  // Returns the global singleton resource tracker for the plugin.
  static PluginResourceTracker* GetInstance();

  // Returns the object associated with the given resource ID, or NULL if
  // there isn't one.
  PluginResource* GetResourceObject(PP_Resource pp_resource);

  void AddResource(PP_Resource pp_resource, linked_ptr<PluginResource> object);

  void AddRefResource(PP_Resource resource);
  void ReleaseResource(PP_Resource resource);

  // Checks if the resource just returned from the renderer is already
  // tracked by the plugin side and adjusts the refcounts if so.
  //
  // When the browser returns a PP_Resource, it could be a new one, in which
  // case the proxy wants to create a corresponding object and call
  // AddResource() on it. However, if the resouce is already known, then the
  // refcount needs to be adjusted in both the plugin and the renderer side
  // and no such object needs to be created.
  //
  // Returns true if the resource was previously known. The refcount will be
  // fixed up such that it's ready to use by the plugin. A proxy can then
  // just return the resource without doing any more work.
  //
  // Typical usage:
  //
  //   PP_Resource result;
  //   dispatcher->Send(new MyMessage(..., &result));
  //   if (PluginResourceTracker::GetInstance()->
  //           PreparePreviouslyTrackedResource(result))
  //     return result;
  //   ... create resource object ...
  //   PluginResourceTracker::GetInstance()->AddResource(result, object);
  //   return result;
  bool PreparePreviouslyTrackedResource(PP_Resource resource);

 private:
  friend struct DefaultSingletonTraits<PluginResourceTracker>;

  PluginResourceTracker();
  ~PluginResourceTracker();

  struct ResourceInfo {
    ResourceInfo();
    ResourceInfo(int ref_count, linked_ptr<PluginResource> r);
    ResourceInfo(const ResourceInfo& other);
    ~ResourceInfo();

    ResourceInfo& operator=(const ResourceInfo& other);

    int ref_count;
    linked_ptr<PluginResource> resource;  // May be NULL.
  };

  void ReleasePluginResourceRef(const PP_Resource& var,
                                bool notify_browser_on_release);

  // Sends a ReleaseResource message to the host corresponding to the given
  // resource.
  void SendReleaseResourceToHost(PP_Resource resource_id,
                                 PluginResource* resource);

  typedef std::map<PP_Resource, ResourceInfo> ResourceMap;
  ResourceMap resource_map_;
};

}  // namespace proxy
}  // namespace pp

#endif  // PPAPI_PROXY_PLUGIN_RESOURCE_TRACKER_H_

// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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

namespace pp {
namespace proxy {

class PluginDispatcher;
class PluginResource;

class PluginResourceTracker {
 public:
  PluginResourceTracker(PluginDispatcher* dispatcher);
  ~PluginResourceTracker();

  // Returns the object associated with the given resource ID, or NULL if
  // there isn't one.
  PluginResource* GetResourceObject(PP_Resource pp_resource);

  void AddResource(PP_Resource pp_resource, linked_ptr<PluginResource> object);

  void AddRefResource(PP_Resource resource);
  void ReleaseResource(PP_Resource resource);

 private:
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

  // Pointer to the dispatcher that owns us.
  PluginDispatcher* dispatcher_;

  typedef std::map<PP_Resource, ResourceInfo> ResourceMap;
  ResourceMap resource_map_;
};

}  // namespace proxy
}  // namespace pp

#endif  // PPAPI_PROXY_PLUGIN_RESOURCE_TRACKER_H_

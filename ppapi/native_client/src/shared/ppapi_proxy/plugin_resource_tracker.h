// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PEPPER_RESOURCE_TRACKER_H_
#define WEBKIT_GLUE_PLUGINS_PEPPER_RESOURCE_TRACKER_H_

#include <map>
#include <utility>

#include "native_client/src/include/nacl_base.h"
#include "native_client/src/include/ref_counted.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_resource.h"

namespace ppapi_proxy {

class PluginResource;

// This class maintains a global list of all live pepper resources. It allows
// us to check resource ID validity and to map them to a specific module.
//
// This object is threadsafe.
class PluginResourceTracker {
 public:
  // Returns the pointer to the singleton object.
  static PluginResourceTracker* Get();

  // PP_Resources --------------------------------------------------------------

  // Increment resource's plugin refcount. See ResourceAndRefCount.
  bool AddRefResource(PP_Resource res);
  bool UnrefResource(PP_Resource res);

 private:
  friend class PluginResource;

  // Prohibit creation other then by the Singleton class.
  PluginResourceTracker();

  // Adds the given resource to the tracker and assigns it a resource ID, local
  // refcount of 1, and initializes the browser reference count to
  // |browser_refcount|. Used only by the Resource class.
  void AddResource(PluginResource* resource, PP_Resource id,
                   size_t browser_refcount);

  // The returned pointer will be NULL if there is no resource. Note that this
  // return value is a scoped_refptr so that we ensure the resource is valid
  // from the point of the lookup to the point that the calling code needs it.
  // Otherwise, the plugin could Release() the resource on another thread and
  // the object will get deleted out from under us.
  scoped_refptr<PluginResource> GetExistingResource(PP_Resource res) const;

  // Get or create a new PluginResource from a browser resource.
  // If we are already tracking this resource, we bump its browser_refcount to
  // reflect that we took ownership of it. If this is a new resource, we create
  // a PluginResource for it with the given browser_refcount.
  template<typename T> scoped_refptr<T> AdoptBrowserResource(
      PP_Resource res, size_t browser_refcount);

  // Try to get a browser-side refcount for an existing resource.
  void ObtainBrowserResource(PP_Resource res);

  // Release browser-side refcount.
  void ReleaseBrowserResource(PP_Resource res, size_t refcount);

  // Last assigned resource ID.
  PP_Resource last_id_;

  struct ResourceAndRefCounts {
    scoped_refptr<PluginResource> resource;
    size_t browser_refcount;
    size_t plugin_refcount;
    ResourceAndRefCounts(PluginResource* r, size_t browser_refcount);
    ~ResourceAndRefCounts();
  };
  typedef std::map<PP_Resource, ResourceAndRefCounts> ResourceMap;
  ResourceMap live_resources_;

  DISALLOW_COPY_AND_ASSIGN(PluginResourceTracker);
};

}  // namespace ppapi_proxy

#endif  // WEBKIT_GLUE_PLUGINS_PEPPER_RESOURCE_TRACKER_H_

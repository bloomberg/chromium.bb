// Copyright (c) 2010 The Native Client Authors. All rights reserved.
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

class PluginInstance;
class PluginModule;
class PluginResource;

// This class maintains a global list of all live pepper resources. It allows
// us to check resource ID validity and to map them to a specific module.
//
// This object is threadsafe.
class PluginResourceTracker {
 public:
  // Returns the pointer to the singleton object.
  static PluginResourceTracker* Get() {
    static PluginResourceTracker tracker;
    return &tracker;
  }

  // PP_Resources --------------------------------------------------------------

  // The returned pointer will be NULL if there is no resource. Note that this
  // return value is a scoped_refptr so that we ensure the resource is valid
  // from the point of the lookup to the point that the calling code needs it.
  // Otherwise, the plugin could Release() the resource on another thread and
  // the object will get deleted out from under us.
  scoped_refptr<PluginResource> GetResource(PP_Resource res) const;

  // Increment resource's plugin refcount. See ResourceAndRefCount comments
  // below.
  bool AddRefResource(PP_Resource res);
  bool UnrefResource(PP_Resource res);

 private:
  friend class PluginResource;

  // Prohibit creation other then by the Singleton class.
  PluginResourceTracker();
  ~PluginResourceTracker();

  // Adds the given resource to the tracker and assigns it a resource ID and
  // refcount of 1. The assigned resource ID will be returned. Used only by the
  // Resource class.
  PP_Resource AddResource(PluginResource* resource);

  // Last assigned resource ID.
  PP_Resource last_id_;

  // For each PP_Resource, keep the Resource* (as refptr) and plugin use count.
  // This use count is different then Resource's RefCount, and is manipulated
  // using this RefResource/UnrefResource. When it drops to zero, we just remove
  // the resource from this resource tracker, but the resource object will be
  // alive so long as some scoped_refptr still holds it's reference. This
  // prevents plugins from forcing destruction of Resource objects.
  typedef std::pair<scoped_refptr<PluginResource>, size_t> ResourceAndRefCount;
  typedef std::map<PP_Resource, ResourceAndRefCount> ResourceMap;
  ResourceMap live_resources_;

  DISALLOW_COPY_AND_ASSIGN(PluginResourceTracker);
};

}  // namespace ppapi_proxy

#endif  // WEBKIT_GLUE_PLUGINS_PEPPER_RESOURCE_TRACKER_H_


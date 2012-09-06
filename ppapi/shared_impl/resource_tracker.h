// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_RESOURCE_TRACKER_H_
#define PPAPI_SHARED_IMPL_RESOURCE_TRACKER_H_

#include <set>

#include "base/basictypes.h"
#include "base/hash_tables.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_checker_impl.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"

namespace ppapi {

class Resource;

class PPAPI_SHARED_EXPORT ResourceTracker {
 public:
  ResourceTracker();
  virtual ~ResourceTracker();

  // The returned pointer will be NULL if there is no resource. The reference
  // count of the resource is unaffected.
  Resource* GetResource(PP_Resource res) const;

  void AddRefResource(PP_Resource res);
  void ReleaseResource(PP_Resource res);

  // Releases a reference on the given resource once the message loop returns.
  void ReleaseResourceSoon(PP_Resource res);

  // Notifies the tracker that a new instance has been created. This must be
  // called before creating any resources associated with the instance.
  void DidCreateInstance(PP_Instance instance);

  // Called when an instance is being deleted. All plugin refs for the
  // associated resources will be force freed, and the resources (if they still
  // exist) will be disassociated from the instance.
  void DidDeleteInstance(PP_Instance instance);

  // Returns the number of resources associated with the given instance.
  // Returns 0 if the instance isn't known.
  int GetLiveObjectsForInstance(PP_Instance instance) const;

 protected:
  // This calls AddResource and RemoveResource.
  friend class Resource;

  // Adds the given resource to the tracker, associating it with the instance
  // stored in the resource object. The new resource ID is returned, and the
  // resource will have 0 plugin refcount. This is called by the resource
  // constructor.
  //
  // Returns 0 if the resource could not be added.
  virtual PP_Resource AddResource(Resource* object);

  // The opposite of AddResource, this removes the tracking information for
  // the given resource. It's called from the resource destructor.
  virtual void RemoveResource(Resource* object);

 private:
  // Calls LastPluginRefWasDeleted on the given resource object and cancels
  // pending callbacks for the resource.
  void LastPluginRefWasDeleted(Resource* object);

  typedef std::set<PP_Resource> ResourceSet;

  struct InstanceData {
    // Lists all resources associated with the given instance as non-owning
    // pointers. This allows us to notify those resources that the instance is
    // going away (otherwise, they may crash if they outlive the instance).
    ResourceSet resources;
  };
  typedef base::hash_map<PP_Instance, linked_ptr<InstanceData> > InstanceMap;

  InstanceMap instance_map_;

  // For each PP_Resource, keep the object pointer and a plugin use count.
  // This use count is different then Resource object's RefCount, and is
  // manipulated using this AddRefResource/UnrefResource. When the plugin use
  // count is positive, we keep an extra ref on the Resource on
  // behalf of the plugin. When it drops to 0, we free that ref, keeping
  // the resource in the list.
  //
  // A resource will be in this list as long as the object is alive.
  typedef std::pair<Resource*, int> ResourceAndRefCount;
  typedef base::hash_map<PP_Resource, ResourceAndRefCount> ResourceMap;
  ResourceMap live_resources_;

  int32 last_resource_value_;

  base::WeakPtrFactory<ResourceTracker> weak_ptr_factory_;

  // TODO(raymes): We won't need to do thread checks once pepper calls are
  // allowed off of the main thread.
  // See http://code.google.com/p/chromium/issues/detail?id=92909.
#ifdef ENABLE_PEPPER_THREADING
  base::ThreadCheckerDoNothing thread_checker_;
#else
  // TODO(raymes): We've seen plugins (Flash) creating resources from random
  // threads. Let's always crash for now in this case. Once we have a handle
  // over how common this is, we can change ThreadCheckerImpl->ThreadChecker
  // so that we only crash in debug mode. See
  // https://code.google.com/p/chromium/issues/detail?id=146415.
  base::ThreadCheckerImpl thread_checker_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ResourceTracker);
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_RESOURCE_TRACKER_H_

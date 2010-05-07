// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PEPPER_RESOURCE_TRACKER_H_
#define WEBKIT_GLUE_PLUGINS_PEPPER_RESOURCE_TRACKER_H_

#include <set>

#include "base/atomic_sequence_num.h"
#include "base/basictypes.h"
#include "base/lock.h"
#include "base/ref_counted.h"
#include "base/singleton.h"

typedef struct _pp_Resource PP_Resource;

namespace pepper {

class Resource;

// This class maintains a global list of all live pepper resources. It allows
// us to check resource ID validity and to map them to a specific module.
//
// This object is threadsafe.
class ResourceTracker {
 public:
  // Returns the pointer to the singleton object.
  static ResourceTracker* Get();

  // The returned pointer will be NULL if there is no resource.
  Resource* GetResource(PP_Resource res) const;

  // Adds the given resource to the tracker and assigns it a resource ID. The
  // assigned resource ID will be returned.
  void AddResource(Resource* resource);

  void DeleteResource(Resource* resource);

 private:
  friend struct DefaultSingletonTraits<ResourceTracker>;

  ResourceTracker();
  ~ResourceTracker();

  // Hold this lock when accessing this object's members.
  mutable Lock lock_;

  typedef std::set<Resource*> ResourceSet;
  ResourceSet live_resources_;

  DISALLOW_COPY_AND_ASSIGN(ResourceTracker);
};

}  // namespace pepper

#endif  // WEBKIT_GLUE_PLUGINS_PEPPER_RESOURCE_TRACKER_H_

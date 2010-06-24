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
#include "third_party/ppapi/c/pp_resource.h"

namespace pepper {

class Buffer;
class DeviceContext2D;
class ImageData;
class Resource;
class URLLoader;
class URLRequestInfo;
class URLResponseInfo;

// This class maintains a global list of all live pepper resources. It allows
// us to check resource ID validity and to map them to a specific module.
//
// This object is threadsafe.
class ResourceTracker {
 public:
  // Returns the pointer to the singleton object.
  static ResourceTracker* Get();

  // The returned pointer will be NULL if there is no resource. Note that this
  // return value is a scoped_refptr so that we ensure the resource is valid
  // from the point of the loopkup to the point that the calling code needs it.
  // Otherwise, the plugin could Release() the resource on another thread and
  // the object will get deleted out from under us.
  scoped_refptr<Resource> GetResource(PP_Resource res) const;

  // Adds the given resource to the tracker and assigns it a resource ID. The
  // assigned resource ID will be returned.
  void AddResource(Resource* resource);

  void DeleteResource(Resource* resource);

  // Helpers for converting resources to a specific type. Returns NULL if the
  // resource is invalid or is a different type.
  scoped_refptr<DeviceContext2D> GetAsDeviceContext2D(PP_Resource res) const;
  scoped_refptr<ImageData> GetAsImageData(PP_Resource res) const;
  scoped_refptr<URLLoader> GetAsURLLoader(PP_Resource res) const;
  scoped_refptr<URLRequestInfo> GetAsURLRequestInfo(PP_Resource res) const;
  scoped_refptr<URLResponseInfo> GetAsURLResponseInfo(PP_Resource res) const;
  scoped_refptr<Buffer> GetAsBuffer(PP_Resource res) const;

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

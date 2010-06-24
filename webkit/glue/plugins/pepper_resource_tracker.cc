// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/pepper_resource_tracker.h"

#include <set>

#include "base/logging.h"
#include "third_party/ppapi/c/pp_resource.h"
#include "webkit/glue/plugins/pepper_buffer.h"
#include "webkit/glue/plugins/pepper_device_context_2d.h"
#include "webkit/glue/plugins/pepper_image_data.h"
#include "webkit/glue/plugins/pepper_resource.h"
#include "webkit/glue/plugins/pepper_url_loader.h"
#include "webkit/glue/plugins/pepper_url_request_info.h"
#include "webkit/glue/plugins/pepper_url_response_info.h"

namespace pepper {

ResourceTracker::ResourceTracker() {
}

ResourceTracker::~ResourceTracker() {
}

// static
ResourceTracker* ResourceTracker::Get() {
  return Singleton<ResourceTracker>::get();
}

scoped_refptr<Resource> ResourceTracker::GetResource(PP_Resource res) const {
  AutoLock lock(lock_);
  Resource* resource = reinterpret_cast<Resource*>(res);
  if (live_resources_.find(resource) == live_resources_.end())
    return scoped_refptr<Resource>();
  return scoped_refptr<Resource>(resource);
}

void ResourceTracker::AddResource(Resource* resource) {
  AutoLock lock(lock_);
  DCHECK(live_resources_.find(resource) == live_resources_.end());
  live_resources_.insert(resource);
}

void ResourceTracker::DeleteResource(Resource* resource) {
  AutoLock lock(lock_);
  ResourceSet::iterator found = live_resources_.find(resource);
  if (found == live_resources_.end()) {
    NOTREACHED();
    return;
  }
  live_resources_.erase(found);
}

scoped_refptr<DeviceContext2D> ResourceTracker::GetAsDeviceContext2D(
    PP_Resource res) const {
  scoped_refptr<Resource> resource = GetResource(res);
  if (!resource.get())
    return scoped_refptr<DeviceContext2D>();
  return scoped_refptr<DeviceContext2D>(resource->AsDeviceContext2D());
}

scoped_refptr<ImageData> ResourceTracker::GetAsImageData(
    PP_Resource res) const {
  scoped_refptr<Resource> resource = GetResource(res);
  if (!resource.get())
    return scoped_refptr<ImageData>();
  return scoped_refptr<ImageData>(resource->AsImageData());
}

scoped_refptr<URLLoader> ResourceTracker::GetAsURLLoader(
    PP_Resource res) const {
  scoped_refptr<Resource> resource = GetResource(res);
  if (!resource.get())
    return scoped_refptr<URLLoader>();
  return scoped_refptr<URLLoader>(resource->AsURLLoader());
}

scoped_refptr<URLRequestInfo> ResourceTracker::GetAsURLRequestInfo(
    PP_Resource res) const {
  scoped_refptr<Resource> resource = GetResource(res);
  if (!resource.get())
    return scoped_refptr<URLRequestInfo>();
  return scoped_refptr<URLRequestInfo>(resource->AsURLRequestInfo());
}

scoped_refptr<URLResponseInfo> ResourceTracker::GetAsURLResponseInfo(
    PP_Resource res) const {
  scoped_refptr<Resource> resource = GetResource(res);
  if (!resource.get())
    return scoped_refptr<URLResponseInfo>();
  return scoped_refptr<URLResponseInfo>(resource->AsURLResponseInfo());
}

scoped_refptr<Buffer> ResourceTracker::GetAsBuffer(
    PP_Resource res) const {
  scoped_refptr<Resource> resource = GetResource(res);
  if (!resource.get())
    return scoped_refptr<Buffer>();
  return scoped_refptr<Buffer>(resource->AsBuffer());
}

}  // namespace pepper

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_RESOURCE_H_
#define WEBKIT_PLUGINS_PPAPI_RESOURCE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/shared_impl/resource.h"
#include "webkit/plugins/ppapi/resource_tracker.h"

namespace webkit {
namespace ppapi {

class Resource : public ::ppapi::Resource {
 public:
  explicit Resource(PluginInstance* instance);
  virtual ~Resource();

  // Returns the instance owning this resource. This is generally to be
  // non-NULL except if the instance is destroyed and some code internal to the
  // PPAPI implementation is keeping a reference for some reason.
  PluginInstance* instance() const { return instance_; }

  // Returns an resource id of this object. If the object doesn't have a
  // resource id, new one is created with plugin refcount of 1. If it does,
  // the refcount is incremented. Use this when you need to return a new
  // reference to the plugin.
  PP_Resource GetReference();

  // When you need to ensure that a resource has a reference, but you do not
  // want to increase the refcount (for example, if you need to call a plugin
  // callback function with a reference), you can use this class. For example:
  //
  // plugin_callback(.., ScopedResourceId(resource).id, ...);
  class ScopedResourceId {
   public:
    explicit ScopedResourceId(Resource* resource)
        : id(resource->GetReference()) {}
    ~ScopedResourceId() {
      ResourceTracker::Get()->ReleaseResource(id);
    }
    const PP_Resource id;
  };

  // Resource implementation.
  virtual void LastPluginRefWasDeleted() OVERRIDE;
  virtual void InstanceWasDeleted() OVERRIDE;

 private:
  // Non-owning pointer to our instance. See getter above.
  PluginInstance* instance_;

  DISALLOW_COPY_AND_ASSIGN(Resource);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_RESOURCE_H_

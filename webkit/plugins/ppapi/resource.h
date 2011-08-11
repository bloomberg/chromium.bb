// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_RESOURCE_H_
#define WEBKIT_PLUGINS_PPAPI_RESOURCE_H_

#include "base/basictypes.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/shared_impl/resource_object_base.h"
#include "webkit/plugins/ppapi/resource_tracker.h"

namespace webkit {
namespace ppapi {

class Resource : public ::ppapi::ResourceObjectBase {
 public:
  explicit Resource(PluginInstance* instance);
  virtual ~Resource();

  // Returns the instance owning this resource. This is generally to be
  // non-NULL except if the instance is destroyed and some code internal to the
  // PPAPI implementation is keeping a reference for some reason.
  PluginInstance* instance() const { return instance_; }

  // Clears the instance pointer when the associated PluginInstance will be
  // destroyed.
  //
  // If you override this, be sure to call the base class' implementation.
  virtual void ClearInstance();

  // Returns an resource id of this object. If the object doesn't have a
  // resource id, new one is created with plugin refcount of 1. If it does,
  // the refcount is incremented. Use this when you need to return a new
  // reference to the plugin.
  PP_Resource GetReference();

  // Returns the resource ID of this object OR NULL IF THERE IS NONE ASSIGNED.
  // This will happen if the plugin doesn't have a reference to the given
  // resource. The resource will not be addref'ed.
  //
  // This should only be used as an input parameter to the plugin for status
  // updates in the proxy layer, where if the plugin has no reference, it will
  // just give up since nothing needs to be updated.
  //
  // Generally you should use GetReference instead. This is why it has this
  // obscure name rather than pp_resource().
  PP_Resource GetReferenceNoAddRef() const;

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
      ResourceTracker::Get()->UnrefResource(id);
    }
    const PP_Resource id;
  };

  // Called by the resource tracker when the last reference from the plugin
  // was released. For a few types of resources, the resource could still
  // stay alive if there are other references held by the PPAPI implementation
  // (possibly for callbacks and things).
  //
  // If you override this, be sure to call the base class' implementation.
  virtual void LastPluginRefWasDeleted();

 private:
  // If referenced by a plugin, holds the id of this resource object. Do not
  // access this member directly, because it is possible that the plugin holds
  // no references to the object, and therefore the resource_id_ is zero. Use
  // either GetReference() to obtain a new resource_id and increase the
  // refcount, or TemporaryReference when you do not want to increase the
  // refcount.
  PP_Resource resource_id_;

  // Non-owning pointer to our instance. See getter above.
  PluginInstance* instance_;

  DISALLOW_COPY_AND_ASSIGN(Resource);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_RESOURCE_H_

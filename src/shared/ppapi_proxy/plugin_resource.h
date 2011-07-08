// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_RESOURCE_H_
#define NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_RESOURCE_H_

#include "native_client/src/include/nacl_base.h"
#include "native_client/src/include/ref_counted.h"
#include "native_client/src/shared/ppapi_proxy/plugin_resource_tracker.h"
#include "native_client/src/third_party/ppapi/c/pp_resource.h"

namespace ppapi_proxy {

// If you inherit from resource, make sure you add the class name here.
#define FOR_ALL_RESOURCES(F) \
  F(PluginAudio) \
  F(PluginAudioConfig) \
  F(PluginBuffer) \
  F(PluginContext3D) \
  F(PluginFont) \
  F(PluginGraphics2D) \
  F(PluginImageData) \
  F(PluginSurface3D)

// Forward declaration of PluginResource classes.
#define DECLARE_RESOURCE_CLASS(RESOURCE) class RESOURCE;
FOR_ALL_RESOURCES(DECLARE_RESOURCE_CLASS)
#undef DECLARE_RESOURCE_CLASS

class PluginResource : public nacl::RefCounted<PluginResource> {
 public:
  PluginResource();
  virtual ~PluginResource();

  // Returns NULL if the resource is invalid or is a different type.
  template<typename T>
  static scoped_refptr<T> GetAs(PP_Resource res) {
    // See if we have the resource cached.
    scoped_refptr<PluginResource> resource =
        PluginResourceTracker::Get()->GetExistingResource(res);

    return resource ? resource->Cast<T>() : NULL;
  }

  // Returns NULL if the resource is invalid or is a different type.
  template<typename T>
  static scoped_refptr<T> AdoptAs(PP_Resource res);

  // Cast the resource into a specified type. This will return NULL if the
  // resource does not match the specified type. Specializations of this
  // template call into As* functions.
  template <typename T> T* Cast() { return NULL; }

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
    explicit ScopedResourceId(PluginResource* resource)
        : id(resource->GetReference()) {}
    ~ScopedResourceId() {
      PluginResourceTracker::Get()->UnrefResource(id);
    }
    const PP_Resource id;
  };

 protected:
  virtual bool InitFromBrowserResource(PP_Resource resource) = 0;

 private:
  // Type-specific getters for individual resource types. These will return
  // NULL if the resource does not match the specified type. Used by the Cast()
  // function.
  #define DEFINE_TYPE_GETTER(RESOURCE)  \
      virtual RESOURCE* As##RESOURCE() { return NULL; }
  FOR_ALL_RESOURCES(DEFINE_TYPE_GETTER)
  #undef DEFINE_TYPE_GETTER

  // Call this macro in the derived class declaration to actually implement the
  // type getter.
  #define IMPLEMENT_RESOURCE(RESOURCE)  \
      virtual RESOURCE* As##RESOURCE() { return this; }

  // If referenced by a plugin, holds the id of this resource object. Do not
  // access this member directly, because it is possible that the plugin holds
  // no references to the object, and therefore the resource_id_ is zero. Use
  // either GetReference() to obtain a new resource_id and increase the
  // refcount, or TemporaryReference when you do not want to increase the
  // refcount.
  PP_Resource resource_id_;

  // Called by the resource tracker when the last plugin reference has been
  // dropped. You cannot use resource_id_ after this function is called!
  friend class PluginResourceTracker;
  void StoppedTracking();

  DISALLOW_COPY_AND_ASSIGN(PluginResource);
};

// Cast() specializations.
#define DEFINE_RESOURCE_CAST(Type)                         \
  template <> inline Type* PluginResource::Cast<Type>() {  \
      return As##Type();                                   \
  }

FOR_ALL_RESOURCES(DEFINE_RESOURCE_CAST)
#undef DEFINE_RESOURCE_CAST

#undef FOR_ALL_RESOURCES

template<typename T> scoped_refptr<T>
PluginResourceTracker::AdoptBrowserResource(PP_Resource res) {
  ResourceMap::iterator result = live_resources_.find(res);
  // Do we have it already?
  if (result == live_resources_.end()) {
    // No - try to create a new one.
    scoped_refptr<T> new_resource = new T();
    if (new_resource->InitFromBrowserResource(res)) {
      AddResource(new_resource, res);
      return new_resource;
    } else {
      return scoped_refptr<T>();
    }
  } else {
    // Consume one more browser refcount.
    ++result->second.browser_refcount;
    return result->second.resource->Cast<T>();
  }
}

template<typename T>
scoped_refptr<T> PluginResource::AdoptAs(PP_Resource res) {
  // Short-circuit if null resource.
  if (!res)
    return NULL;

  // Adopt the resource.
  return PluginResourceTracker::Get()->AdoptBrowserResource<T>(res);
}

}  // namespace ppapi_proxy

#endif  // NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_RESOURCE_H_


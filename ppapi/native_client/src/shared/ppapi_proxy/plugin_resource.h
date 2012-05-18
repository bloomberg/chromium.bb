// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_RESOURCE_H_
#define NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_RESOURCE_H_

#include "native_client/src/include/nacl_base.h"
#include "native_client/src/include/ref_counted.h"
#include "native_client/src/shared/ppapi_proxy/plugin_resource_tracker.h"
#include "ppapi/c/pp_resource.h"

namespace ppapi_proxy {

// If you inherit from resource, make sure you add the class name here.
#define FOR_ALL_RESOURCES(F) \
  F(PluginAudio) \
  F(PluginAudioConfig) \
  F(PluginBuffer) \
  F(PluginFont) \
  F(PluginGraphics2D) \
  F(PluginGraphics3D) \
  F(PluginImageData) \
  F(PluginInputEvent) \
  F(PluginView)

// Forward declaration of PluginResource classes.
#define DECLARE_RESOURCE_CLASS(RESOURCE) class RESOURCE;
FOR_ALL_RESOURCES(DECLARE_RESOURCE_CLASS)
#undef DECLARE_RESOURCE_CLASS

class PluginResource : public nacl::RefCountedThreadSafe<PluginResource> {
 public:
  PluginResource();

  // Returns NULL if the resource is invalid or is a different type.
  template<typename T>
  static scoped_refptr<T> GetAs(PP_Resource res) {
    // See if we have the resource cached.
    scoped_refptr<PluginResource> resource =
        PluginResourceTracker::Get()->GetExistingResource(res);

    return resource ? resource->Cast<T>() : NULL;
  }

  // Adopt the given PP_Resource as type T. Use this function for resources that
  // the browser provides to the plugin with an incremented ref count (i.e.,
  // calls AddRefResource); it initializes the browser refcount to 1 or
  // increments it if the resource already exists.
  // Returns NULL if the resource is invalid or is a different type.
  template<typename T>
  static scoped_refptr<T> AdoptAs(PP_Resource res);

  // Adopt the given PP_Resource as type T. Use this function for resources
  // when the browser drops the refcount immediately. These resources are
  // typically meant to be cached on the plugin side, with little or no
  // interaction back to the browser. For an example, see PluginInputEvent.
  // This is like AdoptAs above, except it initializes the browser_refcount to
  // 0 for resources that are new to the plugin, and does not increment the
  // browser_refcount for resources that exist.
  // Returns NULL if the resource is invalid or is a different type.
  template<typename T>
  static scoped_refptr<T> AdoptAsWithNoBrowserCount(PP_Resource res);

  // Cast the resource into a specified type. This will return NULL if the
  // resource does not match the specified type. Specializations of this
  // template call into As* functions.
  template <typename T> T* Cast() { return NULL; }

 protected:
  virtual ~PluginResource();

  virtual bool InitFromBrowserResource(PP_Resource resource) = 0;

 private:
  friend class nacl::RefCountedThreadSafe<PluginResource>;

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
PluginResourceTracker::AdoptBrowserResource(PP_Resource res,
                                            size_t browser_refcount) {
  ResourceMap::iterator result = live_resources_.find(res);
  // Do we have it already?
  if (result == live_resources_.end()) {
    // No - try to create a new one.
    scoped_refptr<T> new_resource = new T();
    if (new_resource->InitFromBrowserResource(res)) {
      AddResource(new_resource, res, browser_refcount);
      return new_resource;
    } else {
      return scoped_refptr<T>();
    }
  } else {
    // Consume more browser refcounts (unless browser_refcount is 0).
    result->second.browser_refcount += browser_refcount;
    return result->second.resource->Cast<T>();
  }
}

template<typename T>
scoped_refptr<T> PluginResource::AdoptAs(PP_Resource res) {
  // Short-circuit if null resource.
  if (!res)
    return NULL;

  // Adopt the resource with 1 browser-side refcount.
  const size_t browser_refcount = 1;
  return PluginResourceTracker::Get()->AdoptBrowserResource<T>(
      res, browser_refcount);
}

template<typename T>
scoped_refptr<T> PluginResource::AdoptAsWithNoBrowserCount(PP_Resource res) {
  // Short-circuit if null resource.
  if (!res)
    return NULL;

  // Adopt the resource with 0 browser-side refcount.
  const size_t browser_refcount = 0;
  return PluginResourceTracker::Get()->AdoptBrowserResource<T>(
      res, browser_refcount);
}

}  // namespace ppapi_proxy

#endif  // NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_RESOURCE_H_


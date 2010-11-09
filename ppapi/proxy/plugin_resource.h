// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PLUGIN_RESOURCE_H_
#define PPAPI_PROXY_PLUGIN_RESOURCE_H_

#include "base/basictypes.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_resource_tracker.h"

// If you inherit from resource, make sure you add the class name here.
#define FOR_ALL_RESOURCES(F) \
  F(Font) \
  F(Graphics2D) \
  F(ImageData) \
  F(URLLoader) \
  F(URLRequestInfo)\
  F(URLResponseInfo)

namespace pp {
namespace proxy {

// Forward declaration of Resource classes.
#define DECLARE_RESOURCE_CLASS(RESOURCE) class RESOURCE;
FOR_ALL_RESOURCES(DECLARE_RESOURCE_CLASS)
#undef DECLARE_RESOURCE_CLASS

class PluginResource {
 public:
  PluginResource();
  virtual ~PluginResource();

  // Returns NULL if the resource is invalid or is a different type.
  template<typename T> static T* GetAs(PP_Resource res) {
    PluginResource* resource =
        PluginDispatcher::Get()->plugin_resource_tracker()->GetResourceObject(
            res);
    return resource ? resource->Cast<T>() : NULL;
  }

  template <typename T> T* Cast() { return NULL; }

 private:
  // Type-specific getters for individual resource types. These will return
  // NULL if the resource does not match the specified type. Used by the Cast()
  // function.
  #define DEFINE_TYPE_GETTER(RESOURCE)  \
      virtual RESOURCE* As##RESOURCE() { return NULL; }
  FOR_ALL_RESOURCES(DEFINE_TYPE_GETTER)
  #undef DEFINE_TYPE_GETTER

  DISALLOW_COPY_AND_ASSIGN(PluginResource);
};

// Cast() specializations.
#define DEFINE_RESOURCE_CAST(Type)                   \
  template <> inline Type* PluginResource::Cast<Type>() {  \
      return As##Type();                             \
  }
FOR_ALL_RESOURCES(DEFINE_RESOURCE_CAST)
#undef DEFINE_RESOURCE_CAST

}  // namespace proxy
}  // namespace pp

#endif  // PPAPI_PROXY_PLUGIN_RESOURCE_H_

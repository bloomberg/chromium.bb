// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PEPPER_RESOURCE_H_
#define WEBKIT_GLUE_PLUGINS_PEPPER_RESOURCE_H_

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "third_party/ppapi/c/pp_resource.h"
#include "webkit/glue/plugins/pepper_resource_tracker.h"

namespace pepper {

// If you inherit from resource, make sure you add the class name here.
#define FOR_ALL_RESOURCES(F) \
  F(Audio) \
  F(AudioConfig) \
  F(Buffer) \
  F(DirectoryReader) \
  F(FileChooser) \
  F(FileIO) \
  F(FileRef) \
  F(FileSystem) \
  F(Font) \
  F(Graphics2D) \
  F(Graphics3D) \
  F(ImageData) \
  F(ObjectVar) \
  F(PluginModule) \
  F(PrivateFontFile) \
  F(Scrollbar) \
  F(StringVar) \
  F(Transport) \
  F(URLLoader) \
  F(URLRequestInfo) \
  F(URLResponseInfo) \
  F(Var) \
  F(VideoDecoder) \
  F(Widget)

// Forward declaration of Resource classes.
#define DECLARE_RESOURCE_CLASS(RESOURCE) class RESOURCE;
FOR_ALL_RESOURCES(DECLARE_RESOURCE_CLASS)
#undef DECLARE_RESOURCE_CLASS

class Resource : public base::RefCountedThreadSafe<Resource> {
 public:
  explicit Resource(PluginModule* module);
  virtual ~Resource();

  // Returns NULL if the resource is invalid or is a different type.
  template<typename T>
  static scoped_refptr<T> GetAs(PP_Resource res) {
    scoped_refptr<Resource> resource = ResourceTracker::Get()->GetResource(res);
    return resource ? resource->Cast<T>() : NULL;
  }

  PluginModule* module() const { return module_; }

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
    explicit ScopedResourceId(Resource* resource)
        : id(resource->GetReference()) {}
    ~ScopedResourceId() {
      ResourceTracker::Get()->UnrefResource(id);
    }
    const PP_Resource id;
  };

 private:
  // Type-specific getters for individual resource types. These will return
  // NULL if the resource does not match the specified type. Used by the Cast()
  // function.
  #define DEFINE_TYPE_GETTER(RESOURCE)  \
      virtual RESOURCE* As##RESOURCE() { return NULL; }
  FOR_ALL_RESOURCES(DEFINE_TYPE_GETTER)
  #undef DEFINE_TYPE_GETTER

 private:
  // If referenced by a plugin, holds the id of this resource object. Do not
  // access this member directly, because it is possible that the plugin holds
  // no references to the object, and therefore the resource_id_ is zero. Use
  // either GetReference() to obtain a new resource_id and increase the
  // refcount, or TemporaryReference when you do not want to increase the
  // refcount.
  PP_Resource resource_id_;

  // Non-owning pointer to our module.
  PluginModule* module_;

  // Called by the resource tracker when the last plugin reference has been
  // dropped.
  friend class ResourceTracker;
  void StoppedTracking();

  DISALLOW_COPY_AND_ASSIGN(Resource);
};

// Cast() specializations.
#define DEFINE_RESOURCE_CAST(Type)                   \
  template <> inline Type* Resource::Cast<Type>() {  \
      return As##Type();                             \
  }

FOR_ALL_RESOURCES(DEFINE_RESOURCE_CAST)
#undef DEFINE_RESOURCE_CAST

#undef FOR_ALL_RESOURCES
}  // namespace pepper

#endif  // WEBKIT_GLUE_PLUGINS_PEPPER_RESOURCE_H_

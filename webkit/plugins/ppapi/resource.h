// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_RESOURCE_H_
#define WEBKIT_PLUGINS_PPAPI_RESOURCE_H_

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "ppapi/c/pp_resource.h"
#include "webkit/plugins/ppapi/resource_tracker.h"

namespace webkit {
namespace ppapi {

// If you inherit from resource, make sure you add the class name here.
#define FOR_ALL_RESOURCES(F) \
  F(MockResource) \
  F(PPB_AudioConfig_Impl) \
  F(PPB_Audio_Impl) \
  F(PPB_Buffer_Impl) \
  F(PPB_Context3D_Impl) \
  F(PPB_DirectoryReader_Impl) \
  F(PPB_FileChooser_Impl) \
  F(PPB_FileIO_Impl) \
  F(PPB_FileRef_Impl) \
  F(PPB_FileSystem_Impl) \
  F(PPB_Flash_Menu_Impl) \
  F(PPB_Flash_NetConnector_Impl) \
  F(PPB_Font_Impl) \
  F(PPB_Graphics2D_Impl) \
  F(PPB_Graphics3D_Impl) \
  F(PPB_ImageData_Impl) \
  F(PPB_Scrollbar_Impl) \
  F(PPB_Surface3D_Impl) \
  F(PPB_Transport_Impl) \
  F(PPB_URLLoader_Impl) \
  F(PPB_URLRequestInfo_Impl) \
  F(PPB_URLResponseInfo_Impl) \
  F(PPB_VideoDecoder_Impl) \
  F(PPB_Widget_Impl) \
  F(PrivateFontFile)

// Forward declaration of Resource classes.
#define DECLARE_RESOURCE_CLASS(RESOURCE) class RESOURCE;
FOR_ALL_RESOURCES(DECLARE_RESOURCE_CLASS)
#undef DECLARE_RESOURCE_CLASS

class Resource : public base::RefCountedThreadSafe<Resource> {
 public:
  explicit Resource(PluginInstance* instance);
  virtual ~Resource();

  // Returns NULL if the resource is invalid or is a different type.
  template<typename T>
  static scoped_refptr<T> GetAs(PP_Resource res) {
    scoped_refptr<Resource> resource = ResourceTracker::Get()->GetResource(res);
    return resource ? resource->Cast<T>() : NULL;
  }

  // Returns the instance owning this resource. This is generally to be
  // non-NULL except if the instance is destroyed and some code internal to the
  // PPAPI implementation is keeping a reference for some reason.
  PluginInstance* instance() const { return instance_; }

  // Cast the resource into a specified type. This will return NULL if the
  // resource does not match the specified type. Specializations of this
  // template call into As* functions.
  template <typename T> T* Cast() { return NULL; }

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
  // When the plugin instance is deleted, all resources associated with that
  // plugin will have their plugin references force-deleted and this function
  // will be called with instance_destroyed as true. If the plugin normally
  // Release()s a reference before the instance is destroyed,
  // instance_destroyed will be false. It's possible in some rare cases for the
  // plugin to get a new reference to the object in this latter case, if it's
  // stored internal to the PPAPI implementation and returned by some function.
  //
  // If you override this, be sure to call the base class' implementation.
  virtual void LastPluginRefWasDeleted(bool instance_destroyed);

 private:
  // Type-specific getters for individual resource types. These will return
  // NULL if the resource does not match the specified type. Used by the Cast()
  // function.
  #define DEFINE_TYPE_GETTER(RESOURCE)  \
      virtual RESOURCE* As##RESOURCE();
  FOR_ALL_RESOURCES(DEFINE_TYPE_GETTER)
  #undef DEFINE_TYPE_GETTER

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

// Cast() specializations.
#define DEFINE_RESOURCE_CAST(Type)                   \
  template <> inline Type* Resource::Cast<Type>() {  \
    return As##Type();                               \
  }

FOR_ALL_RESOURCES(DEFINE_RESOURCE_CAST)
#undef DEFINE_RESOURCE_CAST

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_RESOURCE_H_

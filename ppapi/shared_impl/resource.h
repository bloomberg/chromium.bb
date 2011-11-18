// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_RESOURCE_H_
#define PPAPI_SHARED_IMPL_RESOURCE_H_

#include <stddef.h>  // For NULL.

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/shared_impl/host_resource.h"

// All resource types should be added here. This implements our hand-rolled
// RTTI system since we don't compile with "real" RTTI.
#define FOR_ALL_PPAPI_RESOURCE_APIS(F) \
  F(PPB_Audio_API) \
  F(PPB_AudioConfig_API) \
  F(PPB_AudioInput_API) \
  F(PPB_AudioInputTrusted_API) \
  F(PPB_AudioTrusted_API) \
  F(PPB_Broker_API) \
  F(PPB_Buffer_API) \
  F(PPB_BufferTrusted_API) \
  F(PPB_Context3D_API) \
  F(PPB_DirectoryReader_API) \
  F(PPB_FileChooser_API) \
  F(PPB_FileIO_API) \
  F(PPB_FileRef_API) \
  F(PPB_FileSystem_API) \
  F(PPB_Find_API) \
  F(PPB_Flash_Menu_API) \
  F(PPB_Flash_NetConnector_API) \
  F(PPB_Font_API) \
  F(PPB_Graphics2D_API) \
  F(PPB_Graphics3D_API) \
  F(PPB_ImageData_API) \
  F(PPB_InputEvent_API) \
  F(PPB_LayerCompositor_API) \
  F(PPB_PDFFont_API) \
  F(PPB_Scrollbar_API) \
  F(PPB_Surface3D_API) \
  F(PPB_TCPSocket_Private_API) \
  F(PPB_Transport_API) \
  F(PPB_UDPSocket_Private_API) \
  F(PPB_URLLoader_API) \
  F(PPB_URLRequestInfo_API) \
  F(PPB_URLResponseInfo_API) \
  F(PPB_VideoCapture_API) \
  F(PPB_VideoDecoder_API) \
  F(PPB_VideoLayer_API) \
  F(PPB_WebSocket_API) \
  F(PPB_Widget_API)

namespace ppapi {

// Forward declare all the resource APIs.
namespace thunk {
#define DECLARE_RESOURCE_CLASS(RESOURCE) class RESOURCE;
FOR_ALL_PPAPI_RESOURCE_APIS(DECLARE_RESOURCE_CLASS)
#undef DECLARE_RESOURCE_CLASS
}  // namespace thunk

class PPAPI_SHARED_EXPORT Resource : public base::RefCounted<Resource> {
 public:
  // For constructing non-proxied objects. This just takes the associated
  // instance, and generates a new resource ID. The host resource will be the
  // same as the newly-generated resource ID.
  explicit Resource(PP_Instance instance);

  // For constructing proxied objects. This takes the resource generated in
  // the host side, stores it, and allocates a "local" resource ID for use in
  // the current process.
  explicit Resource(const HostResource& host_resource);

  virtual ~Resource();

  PP_Instance pp_instance() const { return host_resource_.instance(); }

  // Returns the resource ID for this object in the current process without
  // adjusting the refcount. See also GetReference().
  PP_Resource pp_resource() const { return pp_resource_; }

  // Returns the host resource which identifies the resource in the host side
  // of the process in the case of proxied objects. For in-process objects,
  // this just identifies the in-process resource ID & instance.
  const HostResource& host_resource() { return host_resource_; }

  // Adds a ref on behalf of the plugin and returns the resource ID. This is
  // normally used when returning a resource to the plugin, where it's
  // expecting the returned resource to have ownership of a ref passed.
  // See also pp_resource() to avoid the AddRef.
  PP_Resource GetReference();

  // Called by the resource tracker when the last reference from the plugin
  // was released. For a few types of resources, the resource could still
  // stay alive if there are other references held by the PPAPI implementation
  // (possibly for callbacks and things).
  virtual void LastPluginRefWasDeleted();

  // Called by the resource tracker when the instance is going away but the
  // object is still alive (this is not the common case, since it requires
  // something in the implementation to be keeping a ref that keeps the
  // resource alive.
  //
  // You will want to override this if your resource does some kind of
  // background processing (like maybe network loads) on behalf of the plugin
  // and you want to stop that when the plugin is deleted.
  //
  // Be sure to call this version which clears the instance ID.
  virtual void InstanceWasDeleted();

  // Dynamic casting for this object. Returns the pointer to the given type if
  // it's supported. Derived classes override the functions they support to
  // return the interface.
  #define DEFINE_TYPE_GETTER(RESOURCE) \
    virtual thunk::RESOURCE* As##RESOURCE();
  FOR_ALL_PPAPI_RESOURCE_APIS(DEFINE_TYPE_GETTER)
  #undef DEFINE_TYPE_GETTER

  // Template-based dynamic casting. See specializations below.
  template <typename T> T* GetAs() { return NULL; }

 private:
  // See the getters above.
  PP_Resource pp_resource_;
  HostResource host_resource_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(Resource);
};

// Template-based dynamic casting. These specializations forward to the
// AsXXX virtual functions to return whether the given type is supported.
#define DEFINE_RESOURCE_CAST(RESOURCE) \
  template<> inline thunk::RESOURCE* Resource::GetAs() { \
    return As##RESOURCE(); \
  }
FOR_ALL_PPAPI_RESOURCE_APIS(DEFINE_RESOURCE_CAST)
#undef DEFINE_RESOURCE_CAST

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_RESOURCE_H_

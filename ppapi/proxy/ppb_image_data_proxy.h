// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PPB_IMAGE_DATA_PROXY_H_
#define PPAPI_PPB_IMAGE_DATA_PROXY_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/serialized_structs.h"
#include "ppapi/shared_impl/image_data_impl.h"
#include "ppapi/thunk/ppb_image_data_api.h"

struct PPB_ImageData;
class TransportDIB;

namespace skia {
class PlatformCanvas;
}

namespace pp {
namespace proxy {

class HostResource;

class PPB_ImageData_Proxy : public InterfaceProxy {
 public:
  PPB_ImageData_Proxy(Dispatcher* dispatcher, const void* target_interface);
  virtual ~PPB_ImageData_Proxy();

  static const Info* GetInfo();

  const PPB_ImageData* ppb_image_data_target() const {
    return static_cast<const PPB_ImageData*>(target_interface());
  }

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);
};

class ImageData : public PluginResource,
                  public ::ppapi::thunk::PPB_ImageData_API,
                  public ppapi::ImageDataImpl {
 public:
  ImageData(const HostResource& resource,
            const PP_ImageDataDesc& desc,
            ImageHandle handle);
  virtual ~ImageData();

  // ResourceObjectBase overrides.
  virtual ::ppapi::thunk::PPB_ImageData_API* AsImageData_API();

  // Resource overrides.
  virtual ImageData* AsImageData();

  // PPB_ImageData API.
  virtual PP_Bool Describe(PP_ImageDataDesc* desc);
  virtual void* Map();
  virtual void Unmap();

  skia::PlatformCanvas* mapped_canvas() const { return mapped_canvas_.get(); }

  const PP_ImageDataDesc& desc() const { return desc_; }

  static const ImageHandle NullHandle;
  static ImageHandle HandleFromInt(int32_t i);

 private:
  PP_ImageDataDesc desc_;

  scoped_ptr<TransportDIB> transport_dib_;

  // Null when the image isn't mapped.
  scoped_ptr<skia::PlatformCanvas> mapped_canvas_;

  DISALLOW_COPY_AND_ASSIGN(ImageData);
};

}  // namespace proxy
}  // namespace pp

#endif  // PPAPI_PPB_IMAGE_DATA_PROXY_H_

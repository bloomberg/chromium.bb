// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PPB_IMAGE_DATA_PROXY_H_
#define PPAPI_PPB_IMAGE_DATA_PROXY_H_

#include "base/memory/scoped_ptr.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/proxy/serialized_structs.h"
#include "ppapi/shared_impl/image_data_impl.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/ppb_image_data_api.h"

struct PPB_ImageData;
class TransportDIB;

namespace skia {
class PlatformCanvas;
}

namespace ppapi {

class HostResource;

namespace proxy {

// The proxied image data resource. Unlike most resources, this needs to be
// public in the header since a number of other resources need to access it.
class ImageData : public ppapi::Resource,
                  public ppapi::thunk::PPB_ImageData_API,
                  public ppapi::ImageDataImpl {
 public:
  ImageData(const ppapi::HostResource& resource,
            const PP_ImageDataDesc& desc,
            ImageHandle handle);
  virtual ~ImageData();

  // Resource overrides.
  virtual ppapi::thunk::PPB_ImageData_API* AsPPB_ImageData_API() OVERRIDE;

  // PPB_ImageData API.
  virtual PP_Bool Describe(PP_ImageDataDesc* desc) OVERRIDE;
  virtual void* Map() OVERRIDE;
  virtual void Unmap() OVERRIDE;
  virtual int32_t GetSharedMemory(int* handle, uint32_t* byte_count) OVERRIDE;

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

class PPB_ImageData_Proxy : public InterfaceProxy {
 public:
  PPB_ImageData_Proxy(Dispatcher* dispatcher);
  virtual ~PPB_ImageData_Proxy();

  static PP_Resource CreateProxyResource(PP_Instance instance,
                                         PP_ImageDataFormat format,
                                         const PP_Size& size,
                                         PP_Bool init_to_zero);

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

  static const ApiID kApiID = API_ID_PPB_IMAGE_DATA;

 private:
  // Message handler.
  void OnHostMsgCreate(PP_Instance instance,
                       int32_t format,
                       const PP_Size& size,
                       PP_Bool init_to_zero,
                       HostResource* result,
                       std::string* image_data_desc,
                       ImageHandle* result_image_handle);

  DISALLOW_COPY_AND_ASSIGN(PPB_ImageData_Proxy);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PPB_IMAGE_DATA_PROXY_H_

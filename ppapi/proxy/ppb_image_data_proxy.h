// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PPB_IMAGE_DATA_PROXY_H_
#define PPAPI_PPB_IMAGE_DATA_PROXY_H_

#include "base/memory/scoped_ptr.h"
#include "base/shared_memory.h"
#include "build/build_config.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppb_image_data.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/proxy/serialized_structs.h"
#include "ppapi/shared_impl/ppb_image_data_shared.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/ppb_image_data_api.h"

class TransportDIB;

namespace ppapi {
namespace proxy {

class SerializedHandle;

// The proxied image data resource. Unlike most resources, this needs to be
// public in the header since a number of other resources need to access it.
class ImageData : public ppapi::Resource,
                  public ppapi::thunk::PPB_ImageData_API,
                  public ppapi::PPB_ImageData_Shared {
 public:
#if !defined(OS_NACL)
  ImageData(const ppapi::HostResource& resource,
            const PP_ImageDataDesc& desc,
            ImageHandle handle);
#else
  // In NaCl, we only allow creating an ImageData using a SharedMemoryHandle.
  // ImageHandle can differ by host platform. We need something that is
  // more consistent across platforms for NaCl, so that we can communicate to
  // the host OS in a consistent way.
  ImageData(const ppapi::HostResource& resource,
            const PP_ImageDataDesc& desc,
            const base::SharedMemoryHandle& handle);
#endif
  virtual ~ImageData();

  // Resource overrides.
  virtual ppapi::thunk::PPB_ImageData_API* AsPPB_ImageData_API() OVERRIDE;
  virtual void LastPluginRefWasDeleted() OVERRIDE;

  // PPB_ImageData API.
  virtual PP_Bool Describe(PP_ImageDataDesc* desc) OVERRIDE;
  virtual void* Map() OVERRIDE;
  virtual void Unmap() OVERRIDE;
  virtual int32_t GetSharedMemory(int* handle, uint32_t* byte_count) OVERRIDE;
  virtual SkCanvas* GetPlatformCanvas() OVERRIDE;
  virtual SkCanvas* GetCanvas() OVERRIDE;
  virtual void SetUsedInReplaceContents() OVERRIDE;

  const PP_ImageDataDesc& desc() const { return desc_; }

  // Prepares this image data to be recycled to the plugin. The contents will be
  // cleared if zero_contents is set.
  void RecycleToPlugin(bool zero_contents);

#if !defined(OS_NACL)
  static ImageHandle NullHandle();
  static ImageHandle HandleFromInt(int32_t i);
#endif

 private:
  PP_ImageDataDesc desc_;

#if defined(OS_NACL)
  base::SharedMemory shm_;
  uint32 size_;
  int map_count_;
#else
  scoped_ptr<TransportDIB> transport_dib_;

  // Null when the image isn't mapped.
  scoped_ptr<SkCanvas> mapped_canvas_;
#endif

  // Set to true when this ImageData has been used in a call to
  // Graphics2D.ReplaceContents. This is used to signal that it can be cached.
  bool used_in_replace_contents_;

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
  // Plugin->Host message handlers.
  void OnHostMsgCreate(PP_Instance instance,
                       int32_t format,
                       const PP_Size& size,
                       PP_Bool init_to_zero,
                       HostResource* result,
                       std::string* image_data_desc,
                       ImageHandle* result_image_handle);
  void OnHostMsgCreateNaCl(PP_Instance instance,
                           int32_t format,
                           const PP_Size& size,
                           PP_Bool init_to_zero,
                           HostResource* result,
                           std::string* image_data_desc,
                           ppapi::proxy::SerializedHandle* result_image_handle);

  // Host->Plugin message handlers.
  void OnPluginMsgNotifyUnusedImageData(const HostResource& old_image_data);

  DISALLOW_COPY_AND_ASSIGN(PPB_ImageData_Proxy);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PPB_IMAGE_DATA_PROXY_H_

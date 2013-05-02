// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PPB_IMAGE_DATA_PROXY_H_
#define PPAPI_PPB_IMAGE_DATA_PROXY_H_

#include "base/memory/scoped_ptr.h"
#include "base/shared_memory.h"
#include "build/build_config.h"
#include "ipc/ipc_platform_file.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppb_image_data.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
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
class PPAPI_PROXY_EXPORT ImageData
    : public ppapi::Resource,
      public NON_EXPORTED_BASE(ppapi::thunk::PPB_ImageData_API),
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
  virtual void InstanceWasDeleted() OVERRIDE;

  // PPB_ImageData API.
  virtual PP_Bool Describe(PP_ImageDataDesc* desc) OVERRIDE;
  virtual void* Map() OVERRIDE;
  virtual void Unmap() OVERRIDE;
  virtual int32_t GetSharedMemory(int* handle, uint32_t* byte_count) OVERRIDE;
  virtual SkCanvas* GetPlatformCanvas() OVERRIDE;
  virtual SkCanvas* GetCanvas() OVERRIDE;
  virtual void SetIsCandidateForReuse() OVERRIDE;

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

  // Set to true when this ImageData is a good candidate for reuse.
  bool is_candidate_for_reuse_;

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

  // Utility for creating ImageData resources.
  // This can only be called on the host side of the proxy.
  // On failure, will return invalid resource (0). On success it will return a
  // valid resource and the out params will be written.
  // |desc| contains the result of Describe.
  // |image_handle| and |byte_count| contain the result of GetSharedMemory.
  // NOTE: if |init_to_zero| is false, you should write over the entire image
  // to avoid leaking sensitive data to a less privileged process.
  PPAPI_PROXY_EXPORT static PP_Resource CreateImageData(
      PP_Instance instance,
      PP_ImageDataFormat format,
      const PP_Size& size,
      bool init_to_zero,
      bool is_nacl_plugin,
      PP_ImageDataDesc* desc,
      IPC::PlatformFileForTransit* image_handle,
      uint32_t* byte_count);

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

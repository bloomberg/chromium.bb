// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_image_data_proxy.h"

#include <string.h>  // For memcpy

#include <vector>

#include "base/logging.h"
#include "build/build_config.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/ppb_image_data.h"
#include "ppapi/c/trusted/ppb_image_data_trusted.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/ppapi_messages.h"

#if defined(OS_LINUX)
#include <sys/shm.h>
#endif

namespace pp {
namespace proxy {

class ImageData : public PluginResource {
 public:
  ImageData(const PP_ImageDataDesc& desc, uint64_t memory_handle);
  virtual ~ImageData();

  // Resource overrides.
  virtual ImageData* AsImageData() { return this; }

  void* Map();
  void Unmap();

  const PP_ImageDataDesc& desc() const { return desc_; }

 private:
  PP_ImageDataDesc desc_;
  uint64_t memory_handle_;

  void* mapped_data_;

  DISALLOW_COPY_AND_ASSIGN(ImageData);
};

ImageData::ImageData(const PP_ImageDataDesc& desc,
                     uint64_t memory_handle)
    : desc_(desc),
      memory_handle_(memory_handle),
      mapped_data_(NULL) {
}

ImageData::~ImageData() {
  Unmap();
}

void* ImageData::Map() {
#if defined(OS_LINUX)
  // On linux, the memory handle is a SysV shared memory segment.
  int shmkey = static_cast<int>(memory_handle_);
  void* address = shmat(shmkey, NULL, 0);
  // Mark for deletion in case we crash so the kernel will clean it up.
  shmctl(shmkey, IPC_RMID, 0);
  if (address == (void*)-1)
    return NULL;
  mapped_data_ = address;
  return address;
#else
  NOTIMPLEMENTED();
  return NULL;
#endif
}

void ImageData::Unmap() {
#if defined(OS_LINUX)
  if (mapped_data_)
    shmdt(mapped_data_);
#else
  NOTIMPLEMENTED();
#endif
  mapped_data_ = NULL;
}

namespace {

PP_ImageDataFormat GetNativeImageDataFormat() {
  int32 format = 0;
  PluginDispatcher::Get()->Send(
      new PpapiHostMsg_PPBImageData_GetNativeImageDataFormat(
          INTERFACE_ID_PPB_IMAGE_DATA, &format));
  return static_cast<PP_ImageDataFormat>(format);
}

PP_Bool IsImageDataFormatSupported(PP_ImageDataFormat format) {
  PP_Bool supported = PP_FALSE;
  PluginDispatcher::Get()->Send(
      new PpapiHostMsg_PPBImageData_IsImageDataFormatSupported(
          INTERFACE_ID_PPB_IMAGE_DATA, static_cast<int32_t>(format),
          &supported));
  return supported;
}

PP_Resource Create(PP_Module module_id,
                   PP_ImageDataFormat format,
                   const PP_Size* size,
                   PP_Bool init_to_zero) {
  PP_Resource result = 0;
  std::string image_data_desc;
  uint64_t shm_handle = -1;
  PluginDispatcher::Get()->Send(
      new PpapiHostMsg_PPBImageData_Create(
          INTERFACE_ID_PPB_IMAGE_DATA, module_id, format, *size, init_to_zero,
          &result, &image_data_desc, &shm_handle));

  if (result && image_data_desc.size() == sizeof(PP_ImageDataDesc)) {
    // We serialize the PP_ImageDataDesc just by copying to a string.
    PP_ImageDataDesc desc;
    memcpy(&desc, image_data_desc.data(), sizeof(PP_ImageDataDesc));

    linked_ptr<ImageData> object(
        new ImageData(desc, shm_handle));
    PluginDispatcher::Get()->plugin_resource_tracker()->AddResource(
        result, object);
  }
  return result;
}

PP_Bool IsImageData(PP_Resource resource) {
  ImageData* object = PluginResource::GetAs<ImageData>(resource);
  return BoolToPPBool(!!object);
}

PP_Bool Describe(PP_Resource resource, PP_ImageDataDesc* desc) {
  ImageData* object = PluginResource::GetAs<ImageData>(resource);
  if (!object)
    return PP_FALSE;
  memcpy(desc, &object->desc(), sizeof(PP_ImageDataDesc));
  return PP_TRUE;
}

void* Map(PP_Resource resource) {
  ImageData* object = PluginResource::GetAs<ImageData>(resource);
  if (!object)
    return NULL;
  return object->Map();
}

void Unmap(PP_Resource resource) {
  ImageData* object = PluginResource::GetAs<ImageData>(resource);
  if (object)
    object->Unmap();
}

const PPB_ImageData ppb_imagedata = {
  &GetNativeImageDataFormat,
  &IsImageDataFormatSupported,
  &Create,
  &IsImageData,
  &Describe,
  &Map,
  &Unmap,
};

}  // namespace

PPB_ImageData_Proxy::PPB_ImageData_Proxy(Dispatcher* dispatcher,
                                         const void* target_interface)
    : InterfaceProxy(dispatcher, target_interface) {
}

PPB_ImageData_Proxy::~PPB_ImageData_Proxy() {
}

const void* PPB_ImageData_Proxy::GetSourceInterface() const {
  return &ppb_imagedata;
}

InterfaceID PPB_ImageData_Proxy::GetInterfaceId() const {
  return INTERFACE_ID_PPB_IMAGE_DATA;
}

void PPB_ImageData_Proxy::OnMessageReceived(const IPC::Message& msg) {
  IPC_BEGIN_MESSAGE_MAP(PPB_ImageData_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBImageData_GetNativeImageDataFormat,
                        OnMsgGetNativeImageDataFormat)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBImageData_IsImageDataFormatSupported,
                        OnMsgIsImageDataFormatSupported)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBImageData_Create, OnMsgCreate)
  IPC_END_MESSAGE_MAP()
  // FIXME(brettw) handle bad messages!
}

void PPB_ImageData_Proxy::OnMsgGetNativeImageDataFormat(int32* result) {
  *result = ppb_image_data_target()->GetNativeImageDataFormat();
}

void PPB_ImageData_Proxy::OnMsgIsImageDataFormatSupported(int32 format,
                                                          PP_Bool* result) {
  *result = ppb_image_data_target()->IsImageDataFormatSupported(
      static_cast<PP_ImageDataFormat>(format));
}

void PPB_ImageData_Proxy::OnMsgCreate(PP_Module module,
                                      int32_t format,
                                      const PP_Size& size,
                                      PP_Bool init_to_zero,
                                      PP_Resource* result,
                                      std::string* image_data_desc,
                                      uint64_t* result_shm_handle) {
  *result = ppb_image_data_target()->Create(
      module, static_cast<PP_ImageDataFormat>(format), &size, init_to_zero);
  *result_shm_handle = 0;
  if (*result) {
    // The ImageDesc is just serialized as a string.
    PP_ImageDataDesc desc;
    if (ppb_image_data_target()->Describe(*result, &desc)) {
      image_data_desc->resize(sizeof(PP_ImageDataDesc));
      memcpy(&(*image_data_desc)[0], &desc, sizeof(PP_ImageDataDesc));
    }

    // Get the shared memory handle.
    const PPB_ImageDataTrusted* trusted =
        reinterpret_cast<const PPB_ImageDataTrusted*>(
            dispatcher()->GetLocalInterface(PPB_IMAGEDATA_TRUSTED_INTERFACE));
    uint32_t byte_count = 0;
    if (trusted)
      *result_shm_handle = trusted->GetNativeMemoryHandle(*result, &byte_count);
  }
}

}  // namespace proxy
}  // namespace pp

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_image_data_proxy.h"

#include <string.h>  // For memcpy

#include <vector>

#include "base/logging.h"
#include "build/build_config.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/trusted/ppb_image_data_trusted.h"
#include "ppapi/proxy/host_resource.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/plugin_resource_tracker.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/image_data_impl.h"

#if defined(OS_LINUX)
#include <sys/shm.h>
#elif defined(OS_MACOSX)
#include <sys/stat.h>
#include <sys/mman.h>
#endif

namespace pp {
namespace proxy {

class ImageData : public PluginResource,
                  public pp::shared_impl::ImageDataImpl {
 public:
  ImageData(const HostResource& resource,
            const PP_ImageDataDesc& desc,
            ImageHandle handle);
  virtual ~ImageData();

  // Resource overrides.
  virtual ImageData* AsImageData();

  void* Map();
  void Unmap();

  const PP_ImageDataDesc& desc() const { return desc_; }

  static const ImageHandle NullHandle;
  static ImageHandle HandleFromInt(int32_t i);

 private:
  PP_ImageDataDesc desc_;
  ImageHandle handle_;

  void* mapped_data_;

  DISALLOW_COPY_AND_ASSIGN(ImageData);
};

ImageData::ImageData(const HostResource& resource,
                     const PP_ImageDataDesc& desc,
                     ImageHandle handle)
    : PluginResource(resource),
      desc_(desc),
      handle_(handle),
      mapped_data_(NULL) {
}

ImageData::~ImageData() {
  Unmap();
}

ImageData* ImageData::AsImageData() {
  return this;
}

void* ImageData::Map() {
#if defined(OS_WIN)
  NOTIMPLEMENTED();
  return NULL;
#elif defined(OS_MACOSX)
  struct stat st;
  if (fstat(handle_.fd, &st) != 0)
    return NULL;
  void* memory = mmap(NULL, st.st_size, PROT_READ | PROT_WRITE,
                      MAP_SHARED, handle_.fd, 0);
  if (memory == MAP_FAILED)
    return NULL;
  mapped_data_ = memory;
  return mapped_data_;
#else
  int shmkey = handle_;
  void* address = shmat(shmkey, NULL, 0);
  // Mark for deletion in case we crash so the kernel will clean it up.
  shmctl(shmkey, IPC_RMID, 0);
  if (address == (void*)-1)
    return NULL;
  mapped_data_ = address;
  return address;
#endif
}

void ImageData::Unmap() {
#if defined(OS_WIN)
  NOTIMPLEMENTED();
#elif defined(OS_MACOSX)
  if (mapped_data_) {
    struct stat st;
    if (fstat(handle_.fd, &st) == 0)
      munmap(mapped_data_, st.st_size);
  }
#else
  if (mapped_data_)
    shmdt(mapped_data_);
#endif
  mapped_data_ = NULL;
}

#if defined(OS_WIN)
const ImageHandle ImageData::NullHandle = NULL;
#elif defined(OS_MACOSX)
const ImageHandle ImageData::NullHandle = ImageHandle();
#else
const ImageHandle ImageData::NullHandle = 0;
#endif

ImageHandle ImageData::HandleFromInt(int32_t i) {
#if defined(OS_WIN)
    return reinterpret_cast<ImageHandle>(i);
#elif defined(OS_MACOSX)
    return ImageHandle(i, false);
#else
    return static_cast<ImageHandle>(i);
#endif
}

namespace {

PP_ImageDataFormat GetNativeImageDataFormat() {
  return ImageData::GetNativeImageDataFormat();
}

PP_Bool IsImageDataFormatSupported(PP_ImageDataFormat format) {
  return BoolToPPBool(ImageData::IsImageDataFormatSupported(format));
}

PP_Resource Create(PP_Instance instance,
                   PP_ImageDataFormat format,
                   const PP_Size* size,
                   PP_Bool init_to_zero) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return PP_ERROR_BADARGUMENT;

  HostResource result;
  std::string image_data_desc;
  ImageHandle image_handle = ImageData::NullHandle;
  dispatcher->Send(new PpapiHostMsg_PPBImageData_Create(
      INTERFACE_ID_PPB_IMAGE_DATA, instance, format, *size, init_to_zero,
      &result, &image_data_desc, &image_handle));

  if (result.is_null() || image_data_desc.size() != sizeof(PP_ImageDataDesc))
    return 0;

  // We serialize the PP_ImageDataDesc just by copying to a string.
  PP_ImageDataDesc desc;
  memcpy(&desc, image_data_desc.data(), sizeof(PP_ImageDataDesc));

  linked_ptr<ImageData> object(new ImageData(result, desc, image_handle));
  return PluginResourceTracker::GetInstance()->AddResource(object);
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

const PPB_ImageData image_data_interface = {
  &GetNativeImageDataFormat,
  &IsImageDataFormatSupported,
  &Create,
  &IsImageData,
  &Describe,
  &Map,
  &Unmap,
};

InterfaceProxy* CreateImageDataProxy(Dispatcher* dispatcher,
                                     const void* target_interface) {
  return new PPB_ImageData_Proxy(dispatcher, target_interface);
}

}  // namespace

PPB_ImageData_Proxy::PPB_ImageData_Proxy(Dispatcher* dispatcher,
                                         const void* target_interface)
    : InterfaceProxy(dispatcher, target_interface) {
}

PPB_ImageData_Proxy::~PPB_ImageData_Proxy() {
}

// static
const InterfaceProxy::Info* PPB_ImageData_Proxy::GetInfo() {
  static const Info info = {
    &image_data_interface,
    PPB_IMAGEDATA_INTERFACE,
    INTERFACE_ID_PPB_IMAGE_DATA,
    false,
    &CreateImageDataProxy,
  };
  return &info;
}

bool PPB_ImageData_Proxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_ImageData_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBImageData_Create, OnMsgCreate)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  // FIXME(brettw) handle bad messages!
  return handled;
}

void PPB_ImageData_Proxy::OnMsgCreate(PP_Instance instance,
                                      int32_t format,
                                      const PP_Size& size,
                                      PP_Bool init_to_zero,
                                      HostResource* result,
                                      std::string* image_data_desc,
                                      ImageHandle* result_image_handle) {
  PP_Resource resource = ppb_image_data_target()->Create(
      instance, static_cast<PP_ImageDataFormat>(format), &size, init_to_zero);
  *result_image_handle = ImageData::NullHandle;
  if (resource) {
    // The ImageDesc is just serialized as a string.
    PP_ImageDataDesc desc;
    if (ppb_image_data_target()->Describe(resource, &desc)) {
      image_data_desc->resize(sizeof(PP_ImageDataDesc));
      memcpy(&(*image_data_desc)[0], &desc, sizeof(PP_ImageDataDesc));
    }

    // Get the shared memory handle.
    const PPB_ImageDataTrusted* trusted =
        reinterpret_cast<const PPB_ImageDataTrusted*>(
            dispatcher()->GetLocalInterface(PPB_IMAGEDATA_TRUSTED_INTERFACE));
    uint32_t byte_count = 0;
    if (trusted) {
      int32_t handle;
      if (trusted->GetSharedMemory(resource, &handle, &byte_count) == PP_OK)
          *result_image_handle = ImageData::HandleFromInt(handle);
    }

    result->SetHostResource(instance, resource);
  }
}

}  // namespace proxy
}  // namespace pp

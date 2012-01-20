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
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_resource_tracker.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/host_resource.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/thunk.h"
#include "skia/ext/platform_canvas.h"
#include "ui/gfx/surface/transport_dib.h"

namespace ppapi {
namespace proxy {

ImageData::ImageData(const HostResource& resource,
                     const PP_ImageDataDesc& desc,
                     ImageHandle handle)
    : Resource(resource),
      desc_(desc) {
#if defined(OS_WIN)
  transport_dib_.reset(TransportDIB::CreateWithHandle(handle));
#else
  transport_dib_.reset(TransportDIB::Map(handle));
#endif
}

ImageData::~ImageData() {
}

thunk::PPB_ImageData_API* ImageData::AsPPB_ImageData_API() {
  return this;
}

PP_Bool ImageData::Describe(PP_ImageDataDesc* desc) {
  memcpy(desc, &desc_, sizeof(PP_ImageDataDesc));
  return PP_TRUE;
}

void* ImageData::Map() {
  if (!mapped_canvas_.get()) {
    mapped_canvas_.reset(transport_dib_->GetPlatformCanvas(desc_.size.width,
                                                           desc_.size.height));
    if (!mapped_canvas_.get())
      return NULL;
  }
  const SkBitmap& bitmap =
      skia::GetTopDevice(*mapped_canvas_)->accessBitmap(true);

  bitmap.lockPixels();
  return bitmap.getAddr(0, 0);
}

void ImageData::Unmap() {
  // TODO(brettw) have a way to unmap a TransportDIB. Currently this isn't
  // possible since deleting the TransportDIB also frees all the handles.
  // We need to add a method to TransportDIB to release the handles.
}

int32_t ImageData::GetSharedMemory(int* /* handle */,
                                   uint32_t* /* byte_count */) {
  // Not supported in the proxy (this method is for actually implementing the
  // proxy in the host).
  return PP_ERROR_NOACCESS;
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

PPB_ImageData_Proxy::PPB_ImageData_Proxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher) {
}

PPB_ImageData_Proxy::~PPB_ImageData_Proxy() {
}

// static
PP_Resource PPB_ImageData_Proxy::CreateProxyResource(PP_Instance instance,
                                                     PP_ImageDataFormat format,
                                                     const PP_Size& size,
                                                     PP_Bool init_to_zero) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return 0;

  HostResource result;
  std::string image_data_desc;
  ImageHandle image_handle = ImageData::NullHandle;
  dispatcher->Send(new PpapiHostMsg_PPBImageData_Create(
      kApiID, instance, format, size, init_to_zero,
      &result, &image_data_desc, &image_handle));

  if (result.is_null() || image_data_desc.size() != sizeof(PP_ImageDataDesc))
    return 0;

  // We serialize the PP_ImageDataDesc just by copying to a string.
  PP_ImageDataDesc desc;
  memcpy(&desc, image_data_desc.data(), sizeof(PP_ImageDataDesc));

  return (new ImageData(result, desc, image_handle))->GetReference();
}

bool PPB_ImageData_Proxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_ImageData_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBImageData_Create, OnHostMsgCreate)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PPB_ImageData_Proxy::OnHostMsgCreate(PP_Instance instance,
                                          int32_t format,
                                          const PP_Size& size,
                                          PP_Bool init_to_zero,
                                          HostResource* result,
                                          std::string* image_data_desc,
                                          ImageHandle* result_image_handle) {
  *result_image_handle = ImageData::NullHandle;

  thunk::EnterResourceCreation enter(instance);
  if (enter.failed())
    return;

  PP_Resource resource = enter.functions()->CreateImageData(
      instance, static_cast<PP_ImageDataFormat>(format), size, init_to_zero);
  if (!resource)
    return;
  result->SetHostResource(instance, resource);

  // Get the description, it's just serialized as a string.
  thunk::EnterResourceNoLock<thunk::PPB_ImageData_API> enter_resource(
      resource, false);
  PP_ImageDataDesc desc;
  if (enter_resource.object()->Describe(&desc) == PP_TRUE) {
    image_data_desc->resize(sizeof(PP_ImageDataDesc));
    memcpy(&(*image_data_desc)[0], &desc, sizeof(PP_ImageDataDesc));
  }

  // Get the shared memory handle.
  uint32_t byte_count = 0;
  int32_t handle = 0;
  if (enter_resource.object()->GetSharedMemory(&handle, &byte_count) == PP_OK) {
#if defined(OS_WIN)
    ImageHandle ih = ImageData::HandleFromInt(handle);
    *result_image_handle = dispatcher()->ShareHandleWithRemote(ih, false);
#else
    *result_image_handle = ImageData::HandleFromInt(handle);
#endif
  }
}

}  // namespace proxy
}  // namespace ppapi

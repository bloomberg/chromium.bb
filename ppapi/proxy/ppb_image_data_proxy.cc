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
#include "ppapi/proxy/image_data.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_resource_tracker.h"
#include "ppapi/proxy/ppapi_messages.h"

namespace pp {
namespace proxy {

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

  PP_Resource result = 0;
  std::string image_data_desc;
  ImageHandle image_handle = ImageData::NullHandle;
  dispatcher->Send(new PpapiHostMsg_PPBImageData_Create(
      INTERFACE_ID_PPB_IMAGE_DATA, instance, format, *size, init_to_zero,
      &result, &image_data_desc, &image_handle));

  if (result && image_data_desc.size() == sizeof(PP_ImageDataDesc)) {
    // We serialize the PP_ImageDataDesc just by copying to a string.
    PP_ImageDataDesc desc;
    memcpy(&desc, image_data_desc.data(), sizeof(PP_ImageDataDesc));

    linked_ptr<ImageData> object(new ImageData(instance, desc, image_handle));
    PluginResourceTracker::GetInstance()->AddResource(result, object);
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
                                      PP_Resource* result,
                                      std::string* image_data_desc,
                                      ImageHandle* result_image_handle) {
  *result = ppb_image_data_target()->Create(
      instance, static_cast<PP_ImageDataFormat>(format), &size, init_to_zero);
  *result_image_handle = ImageData::NullHandle;
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
    if (trusted) {
      int32_t handle;
      if (trusted->GetSharedMemory(*result, &handle, &byte_count) == PP_OK)
          *result_image_handle = ImageData::HandleFromInt(handle);
    }
  }
}

}  // namespace proxy
}  // namespace pp

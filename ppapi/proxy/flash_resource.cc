// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/flash_resource.h"

#include "ipc/ipc_message.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/proxy/dispatch_reply_message.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/array_writer.h"
#include "ppapi/thunk/enter.h"

namespace ppapi {
namespace proxy {

FlashResource::FlashResource(Connection connection, PP_Instance instance)
    : PluginResource(connection, instance) {
  SendCreateToRenderer(PpapiHostMsg_Flash_Create());
}

FlashResource::~FlashResource() {
}

thunk::PPB_Flash_Functions_API* FlashResource::AsPPB_Flash_Functions_API() {
  return this;
}

int32_t FlashResource::EnumerateVideoCaptureDevices(
    PP_Instance instance,
    PP_Resource video_capture,
    const PP_ArrayOutput& devices) {
  ArrayWriter output;
  output.set_pp_array_output(devices);
  if (!output.is_valid())
    return PP_ERROR_BADARGUMENT;

  thunk::EnterResource<thunk::PPB_VideoCapture_API> enter(video_capture, true);
  if (enter.failed())
    return PP_ERROR_NOINTERFACE;

  std::vector<ppapi::DeviceRefData> device_ref_data;
  int32_t result =
      SyncCall<PpapiPluginMsg_Flash_EnumerateVideoCaptureDevicesReply>(
          RENDERER,
          PpapiHostMsg_Flash_EnumerateVideoCaptureDevices(
              enter.resource()->host_resource()),
          &device_ref_data);
  if (result != PP_OK)
    return result;

  std::vector<scoped_refptr<Resource> > device_resources;
  for (size_t i = 0; i < device_ref_data.size(); ++i) {
    scoped_refptr<Resource> resource(new PPB_DeviceRef_Shared(
        OBJECT_IS_PROXY, instance, device_ref_data[i]));
    device_resources.push_back(resource);
  }

  if (!output.StoreResourceVector(device_resources))
    return PP_ERROR_FAILED;

  return PP_OK;
}

}  // namespace proxy
}  // namespace ppapi

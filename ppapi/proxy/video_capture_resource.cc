// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/video_capture_resource.h"

#include "ppapi/c/dev/ppp_video_capture_dev.h"
#include "ppapi/proxy/dispatch_reply_message.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_globals.h"
#include "ppapi/proxy/plugin_resource_tracker.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppb_buffer_proxy.h"
#include "ppapi/proxy/resource_message_params.h"
#include "ppapi/shared_impl/array_writer.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/shared_impl/tracked_callback.h"

namespace ppapi {
namespace proxy {

VideoCaptureResource::VideoCaptureResource(
    Connection connection,
    PP_Instance instance,
    PluginDispatcher* dispatcher)
    : PluginResource(connection, instance),
      open_state_(BEFORE_OPEN),
      has_pending_enum_devices_callback_(false) {
  SendCreate(RENDERER, PpapiHostMsg_VideoCapture_Create());

  ppp_video_capture_impl_ = static_cast<const PPP_VideoCapture_Dev*>(
      dispatcher->local_get_interface()(PPP_VIDEO_CAPTURE_DEV_INTERFACE));
}

VideoCaptureResource::~VideoCaptureResource() {
}

void VideoCaptureResource::OnReplyReceived(
    const ResourceMessageReplyParams& params,
    const IPC::Message& msg) {
  if (params.sequence()) {
    PluginResource::OnReplyReceived(params, msg);
    return;
  }

  IPC_BEGIN_MESSAGE_MAP(VideoCaptureResource, msg)
    PPAPI_DISPATCH_PLUGIN_RESOURCE_CALL(
        PpapiPluginMsg_VideoCapture_OnDeviceInfo,
        OnPluginMsgOnDeviceInfo)
    PPAPI_DISPATCH_PLUGIN_RESOURCE_CALL(
        PpapiPluginMsg_VideoCapture_OnStatus,
        OnPluginMsgOnStatus)
    PPAPI_DISPATCH_PLUGIN_RESOURCE_CALL(
        PpapiPluginMsg_VideoCapture_OnError,
        OnPluginMsgOnError)
    PPAPI_DISPATCH_PLUGIN_RESOURCE_CALL(
        PpapiPluginMsg_VideoCapture_OnBufferReady,
        OnPluginMsgOnBufferReady)
    PPAPI_DISPATCH_PLUGIN_RESOURCE_CALL_UNHANDLED(NOTREACHED())
  IPC_END_MESSAGE_MAP()
}

int32_t VideoCaptureResource::EnumerateDevices(
    PP_Resource* devices,
    scoped_refptr<TrackedCallback> callback) {
  if (has_pending_enum_devices_callback_)
    return PP_ERROR_INPROGRESS;

  has_pending_enum_devices_callback_ = true;

  Call<PpapiPluginMsg_VideoCapture_EnumerateDevicesReply>(
      RENDERER,
      PpapiHostMsg_VideoCapture_EnumerateDevices(),
      base::Bind(&VideoCaptureResource::OnPluginMsgEnumerateDevicesReply,
                 this,
                 devices,
                 callback));
  return PP_OK_COMPLETIONPENDING;
}

int32_t VideoCaptureResource::Open(
    const std::string& device_id,
    const PP_VideoCaptureDeviceInfo_Dev& requested_info,
    uint32_t buffer_count,
    scoped_refptr<TrackedCallback> callback) {
  if (open_state_ != BEFORE_OPEN)
    return PP_ERROR_FAILED;

  if (TrackedCallback::IsPending(open_callback_))
    return PP_ERROR_INPROGRESS;

  open_callback_ = callback;

  Call<PpapiPluginMsg_VideoCapture_OpenReply>(
      RENDERER,
      PpapiHostMsg_VideoCapture_Open(device_id, requested_info, buffer_count),
      base::Bind(&VideoCaptureResource::OnPluginMsgOpenReply, this));
  return PP_OK_COMPLETIONPENDING;
}

int32_t VideoCaptureResource::StartCapture() {
  if (open_state_ != OPENED)
    return PP_ERROR_FAILED;

  buffer_in_use_.clear();
  Post(RENDERER, PpapiHostMsg_VideoCapture_StartCapture());
  return PP_OK;
}

int32_t VideoCaptureResource::ReuseBuffer(uint32_t buffer) {
  if (buffer >= buffer_in_use_.size() || !buffer_in_use_[buffer])
    return PP_ERROR_BADARGUMENT;
  Post(RENDERER, PpapiHostMsg_VideoCapture_ReuseBuffer(buffer));
  return PP_OK;
}

int32_t VideoCaptureResource::StopCapture() {
  if (open_state_ != OPENED)
    return PP_ERROR_FAILED;

  Post(RENDERER, PpapiHostMsg_VideoCapture_StopCapture());
  return PP_OK;
}

void VideoCaptureResource::Close() {
  if (open_state_ == CLOSED)
    return;

  Post(RENDERER, PpapiHostMsg_VideoCapture_Close());

  open_state_ = CLOSED;

  if (TrackedCallback::IsPending(open_callback_))
    open_callback_->PostAbort();
}

int32_t VideoCaptureResource::EnumerateDevicesSync(
    const PP_ArrayOutput& devices) {
  ArrayWriter output;
  output.set_pp_array_output(devices);
  if (!output.is_valid())
    return PP_ERROR_BADARGUMENT;

  std::vector<ppapi::DeviceRefData> device_ref_data;
  int32_t result = SyncCall<PpapiPluginMsg_VideoCapture_EnumerateDevicesReply>(
      RENDERER,
      PpapiHostMsg_VideoCapture_EnumerateDevices(),
      &device_ref_data);

  std::vector<scoped_refptr<Resource> > device_resources;
  for (size_t i = 0; i < device_ref_data.size(); ++i) {
    scoped_refptr<Resource> resource(new PPB_DeviceRef_Shared(
        OBJECT_IS_PROXY, pp_instance(), device_ref_data[i]));
    device_resources.push_back(resource);
  }

  if (!output.StoreResourceVector(device_resources))
    return PP_ERROR_FAILED;

  return result;
}

void VideoCaptureResource::OnPluginMsgOnDeviceInfo(
    const ResourceMessageReplyParams& params,
    const struct PP_VideoCaptureDeviceInfo_Dev& info,
    const std::vector<HostResource>& buffers,
    uint32_t buffer_size) {
  if (!ppp_video_capture_impl_)
    return;

  std::vector<base::SharedMemoryHandle> handles;
  params.TakeAllSharedMemoryHandles(&handles);
  CHECK(handles.size() == buffers.size());

  PluginResourceTracker* tracker =
      PluginGlobals::Get()->plugin_resource_tracker();
  scoped_array<PP_Resource> resources(new PP_Resource[buffers.size()]);
  for (size_t i = 0; i < buffers.size(); ++i) {
    // We assume that the browser created a new set of resources.
    DCHECK(!tracker->PluginResourceForHostResource(buffers[i]));
    resources[i] = ppapi::proxy::PPB_Buffer_Proxy::AddProxyResource(
        buffers[i], handles[i], buffer_size);
  }

  buffer_in_use_ = std::vector<bool>(buffers.size());

  CallWhileUnlocked(ppp_video_capture_impl_->OnDeviceInfo,
                    pp_instance(),
                    pp_resource(),
                    &info,
                    static_cast<uint32_t>(buffers.size()),
                    const_cast<const PP_Resource*>(resources.get()));

  for (size_t i = 0; i < buffers.size(); ++i)
    tracker->ReleaseResource(resources[i]);
}

void VideoCaptureResource::OnPluginMsgOnStatus(
    const ResourceMessageReplyParams& params,
    uint32_t status) {
  switch (status) {
    case PP_VIDEO_CAPTURE_STATUS_STARTING:
    case PP_VIDEO_CAPTURE_STATUS_STOPPING:
      // Those states are not sent by the browser.
      NOTREACHED();
      break;
  }
  if (ppp_video_capture_impl_) {
    CallWhileUnlocked(ppp_video_capture_impl_->OnStatus,
                      pp_instance(),
                      pp_resource(),
                      status);
  }
}

void VideoCaptureResource::OnPluginMsgOnError(
    const ResourceMessageReplyParams& params,
    uint32_t error_code) {
  open_state_ = CLOSED;
  if (ppp_video_capture_impl_) {
    CallWhileUnlocked(ppp_video_capture_impl_->OnError,
                      pp_instance(),
                      pp_resource(),
                      error_code);
  }
}

void VideoCaptureResource::OnPluginMsgOnBufferReady(
    const ResourceMessageReplyParams& params,
    uint32_t buffer) {
  SetBufferInUse(buffer);
  if (ppp_video_capture_impl_) {
    CallWhileUnlocked(ppp_video_capture_impl_->OnBufferReady,
                      pp_instance(),
                      pp_resource(),
                      buffer);
  }
}

void VideoCaptureResource::OnPluginMsgOpenReply(
    const ResourceMessageReplyParams& params) {
  if (open_state_ == BEFORE_OPEN && params.result() == PP_OK)
    open_state_ = OPENED;

  // The callback may have been aborted by Close().
  if (TrackedCallback::IsPending(open_callback_))
    open_callback_->Run(params.result());
}

void VideoCaptureResource::OnPluginMsgEnumerateDevicesReply(
    PP_Resource* devices_output,
    scoped_refptr<TrackedCallback> callback,
    const ResourceMessageReplyParams& params,
    const std::vector<DeviceRefData>& devices) {
  if (!TrackedCallback::IsPending(callback))
    return;

  DCHECK(has_pending_enum_devices_callback_);
  has_pending_enum_devices_callback_ = false;

  if (params.result() == PP_OK && devices_output) {
    // devices_output points to the resource array of PP_ArrayOutput.  In C++,
    // it's typically allocated in VideoCapture_Dev.
    *devices_output = PPB_DeviceRef_Shared::CreateResourceArray(
        OBJECT_IS_PROXY, pp_instance(), devices);
  }

  callback->Run(params.result());
}

void VideoCaptureResource::SetBufferInUse(uint32_t buffer_index) {
  DCHECK(buffer_index < buffer_in_use_.size());
  buffer_in_use_[buffer_index] = true;
}

}  // namespace proxy
}  // namespace ppapi

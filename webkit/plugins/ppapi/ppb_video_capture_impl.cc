// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_video_capture_impl.h"

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "ppapi/c/dev/pp_video_capture_dev.h"
#include "ppapi/c/dev/ppb_video_capture_dev.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/ppb_device_ref_shared.h"
#include "ppapi/shared_impl/resource_tracker.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/plugin_module.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/ppb_buffer_impl.h"
#include "webkit/plugins/ppapi/resource_helper.h"

using ppapi::DeviceRefData;
using ppapi::PpapiGlobals;
using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_Buffer_API;
using ppapi::thunk::PPB_VideoCapture_API;
using ppapi::TrackedCallback;

namespace {

// Maximum number of buffers to actually allocate.
const uint32_t kMaxBuffers = 20;

}  // namespace

namespace webkit {
namespace ppapi {

PPB_VideoCapture_Impl::PPB_VideoCapture_Impl(PP_Instance instance)
    : PPB_VideoCapture_Shared(instance),
      buffer_count_hint_(0),
      ppp_videocapture_(NULL),
      capability_() {
}

PPB_VideoCapture_Impl::~PPB_VideoCapture_Impl() {
  Close();
}

bool PPB_VideoCapture_Impl::Init() {
  PluginInstance* instance = ResourceHelper::GetPluginInstance(this);
  if (!instance)
    return false;
  ppp_videocapture_ = static_cast<const PPP_VideoCapture_Dev*>(
      instance->module()->GetPluginInterface(PPP_VIDEO_CAPTURE_DEV_INTERFACE));
  if (!ppp_videocapture_)
    return false;

  return true;
}

void PPB_VideoCapture_Impl::OnStarted(media::VideoCapture* capture) {
  if (SetStatus(PP_VIDEO_CAPTURE_STATUS_STARTED, false))
    SendStatus();
}

void PPB_VideoCapture_Impl::OnStopped(media::VideoCapture* capture) {
  if (SetStatus(PP_VIDEO_CAPTURE_STATUS_STOPPED, false))
    SendStatus();
}

void PPB_VideoCapture_Impl::OnPaused(media::VideoCapture* capture) {
  if (SetStatus(PP_VIDEO_CAPTURE_STATUS_PAUSED, false))
    SendStatus();
}

void PPB_VideoCapture_Impl::OnError(media::VideoCapture* capture,
                                    int error_code) {
  // Today, the media layer only sends "1" as an error.
  DCHECK(error_code == 1);
  // It either comes because some error was detected while starting (e.g. 2
  // conflicting "master" resolution), or because the browser failed to start
  // the capture.
  SetStatus(PP_VIDEO_CAPTURE_STATUS_STOPPED, true);
  ppp_videocapture_->OnError(pp_instance(), pp_resource(), PP_ERROR_FAILED);
}

void PPB_VideoCapture_Impl::OnRemoved(media::VideoCapture* capture) {
}

void PPB_VideoCapture_Impl::OnBufferReady(
    media::VideoCapture* capture,
    scoped_refptr<media::VideoCapture::VideoFrameBuffer> buffer) {
  DCHECK(buffer.get());
  for (uint32_t i = 0; i < buffers_.size(); ++i) {
    if (!buffers_[i].in_use) {
      // TODO(ihf): Switch to a size calculation based on stride.
      // Stride is filled out now but not more meaningful than size
      // until wjia unifies VideoFrameBuffer and media::VideoFrame.
      size_t size = std::min(static_cast<size_t>(buffers_[i].buffer->size()),
          buffer->buffer_size);
      memcpy(buffers_[i].data, buffer->memory_pointer, size);
      buffers_[i].in_use = true;
      platform_video_capture_->FeedBuffer(buffer);
      ppp_videocapture_->OnBufferReady(pp_instance(), pp_resource(), i);
      return;
    }
  }
}

void PPB_VideoCapture_Impl::OnDeviceInfoReceived(
    media::VideoCapture* capture,
    const media::VideoCaptureParams& device_info) {
  PP_VideoCaptureDeviceInfo_Dev info = {
    static_cast<uint32_t>(device_info.width),
    static_cast<uint32_t>(device_info.height),
    static_cast<uint32_t>(device_info.frame_per_second)
  };
  ReleaseBuffers();

  // Allocate buffers. We keep a reference to them, that is released in
  // ReleaseBuffers.
  // YUV 4:2:0
  int uv_width = info.width / 2;
  int uv_height = info.height / 2;
  size_t size = info.width * info.height + 2 * uv_width * uv_height;
  scoped_array<PP_Resource> resources(new PP_Resource[buffer_count_hint_]);

  buffers_.reserve(buffer_count_hint_);
  for (size_t i = 0; i < buffer_count_hint_; ++i) {
    resources[i] = PPB_Buffer_Impl::Create(pp_instance(), size);
    if (!resources[i])
      break;

    EnterResourceNoLock<PPB_Buffer_API> enter(resources[i], true);
    DCHECK(enter.succeeded());

    BufferInfo info;
    info.buffer = static_cast<PPB_Buffer_Impl*>(enter.object());
    info.data = info.buffer->Map();
    if (!info.data) {
      PpapiGlobals::Get()->GetResourceTracker()->ReleaseResource(resources[i]);
      break;
    }
    buffers_.push_back(info);
  }

  if (buffers_.empty()) {
    // We couldn't allocate/map buffers at all. Send an error and stop the
    // capture.
    ppp_videocapture_->OnError(pp_instance(), pp_resource(), PP_ERROR_NOMEMORY);
    SetStatus(PP_VIDEO_CAPTURE_STATUS_STOPPING, true);
    platform_video_capture_->StopCapture(this);
    return;
  }

  ppp_videocapture_->OnDeviceInfo(pp_instance(), pp_resource(), &info,
                                  buffers_.size(), resources.get());
}

void PPB_VideoCapture_Impl::OnInitialized(media::VideoCapture* capture,
                                          bool succeeded) {
  DCHECK(capture == platform_video_capture_.get());

  OnOpenComplete(succeeded ? PP_OK : PP_ERROR_FAILED);
}

int32_t PPB_VideoCapture_Impl::InternalEnumerateDevices(
    PP_Resource* devices,
    scoped_refptr<TrackedCallback> callback) {
  PluginInstance* instance = ResourceHelper::GetPluginInstance(this);
  if (!instance)
    return PP_ERROR_FAILED;

  devices_ = devices;
  enumerate_devices_callback_ = callback;
  instance->delegate()->EnumerateDevices(
      PP_DEVICETYPE_DEV_VIDEOCAPTURE,
      base::Bind(&PPB_VideoCapture_Impl::EnumerateDevicesCallbackFunc,
                 AsWeakPtr()));
  return PP_OK_COMPLETIONPENDING;
}

int32_t PPB_VideoCapture_Impl::InternalOpen(
    const std::string& device_id,
    const PP_VideoCaptureDeviceInfo_Dev& requested_info,
    uint32_t buffer_count,
    scoped_refptr<TrackedCallback> callback) {
  // It is able to complete synchronously if the default device is used.
  bool sync_completion = device_id.empty();

  PluginInstance* instance = ResourceHelper::GetPluginInstance(this);
  if (!instance)
    return PP_ERROR_FAILED;

  SetRequestedInfo(requested_info, buffer_count);

  DCHECK(!platform_video_capture_.get());
  platform_video_capture_ =
      instance->delegate()->CreateVideoCapture(device_id, this);

  if (sync_completion) {
    OnInitialized(platform_video_capture_.get(), true);
    return PP_OK;
  } else {
    open_callback_ = callback;
    return PP_OK_COMPLETIONPENDING;
  }
}

int32_t PPB_VideoCapture_Impl::InternalStartCapture() {
  DCHECK(buffers_.empty());
  platform_video_capture_->StartCapture(this, capability_);
  return PP_OK;
}

int32_t PPB_VideoCapture_Impl::InternalReuseBuffer(uint32_t buffer) {
  if (buffer >= buffers_.size() || !buffers_[buffer].in_use)
    return PP_ERROR_BADARGUMENT;
  buffers_[buffer].in_use = false;
  return PP_OK;
}

int32_t PPB_VideoCapture_Impl::InternalStopCapture() {
  ReleaseBuffers();
  platform_video_capture_->StopCapture(this);
  return PP_OK;
}

void PPB_VideoCapture_Impl::InternalClose() {
  StopCapture();
  DCHECK(buffers_.empty());

  DetachPlatformVideoCapture();
}

int32_t PPB_VideoCapture_Impl::InternalStartCapture0_1(
    const PP_VideoCaptureDeviceInfo_Dev& requested_info,
    uint32_t buffer_count) {
  PluginInstance* instance = ResourceHelper::GetPluginInstance(this);
  if (!instance) {
    SetStatus(PP_VIDEO_CAPTURE_STATUS_STOPPED, true);
    return PP_ERROR_FAILED;
  }

  DCHECK(buffers_.empty());

  SetRequestedInfo(requested_info, buffer_count);

  DetachPlatformVideoCapture();
  platform_video_capture_ =
      instance->delegate()->CreateVideoCapture("", this);
  platform_video_capture_->StartCapture(this, capability_);

  return PP_OK;
}

const PPB_VideoCapture_Impl::DeviceRefDataVector&
    PPB_VideoCapture_Impl::InternalGetDeviceRefData() const {
  return devices_data_;
}

void PPB_VideoCapture_Impl::ReleaseBuffers() {
  ::ppapi::ResourceTracker* tracker = PpapiGlobals::Get()->GetResourceTracker();
  for (size_t i = 0; i < buffers_.size(); ++i) {
    buffers_[i].buffer->Unmap();
    tracker->ReleaseResource(buffers_[i].buffer->pp_resource());
  }
  buffers_.clear();
}

void PPB_VideoCapture_Impl::SendStatus() {
  ppp_videocapture_->OnStatus(pp_instance(), pp_resource(), status_);
}

void PPB_VideoCapture_Impl::SetRequestedInfo(
    const PP_VideoCaptureDeviceInfo_Dev& device_info,
    uint32_t buffer_count) {
  // Clamp the buffer count to between 1 and |kMaxBuffers|.
  buffer_count_hint_ = std::min(std::max(buffer_count, 1U), kMaxBuffers);

  capability_.width = device_info.width;
  capability_.height = device_info.height;
  capability_.frame_rate = device_info.frames_per_second;
  capability_.expected_capture_delay = 0;  // Ignored.
  capability_.color = media::VideoCaptureCapability::kI420;
  capability_.interlaced = false;  // Ignored.
}

void PPB_VideoCapture_Impl::DetachPlatformVideoCapture() {
  if (platform_video_capture_.get()) {
    platform_video_capture_->DetachEventHandler();
    platform_video_capture_ = NULL;
  }
}

void PPB_VideoCapture_Impl::EnumerateDevicesCallbackFunc(
    int request_id,
    bool succeeded,
    const DeviceRefDataVector& devices) {
  devices_data_.clear();
  if (succeeded)
    devices_data_ = devices;

  OnEnumerateDevicesComplete(succeeded ? PP_OK : PP_ERROR_FAILED, devices);
}

PPB_VideoCapture_Impl::BufferInfo::BufferInfo()
    : in_use(false),
      data(NULL),
      buffer() {
}

PPB_VideoCapture_Impl::BufferInfo::~BufferInfo() {
}

}  // namespace ppapi
}  // namespace webkit

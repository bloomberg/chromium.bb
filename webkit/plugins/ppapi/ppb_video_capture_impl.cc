// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_video_capture_impl.h"

#include <algorithm>
#include <string>

#include "base/logging.h"
#include "ppapi/c/dev/pp_video_capture_dev.h"
#include "ppapi/c/dev/ppb_video_capture_dev.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/thunk/enter.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/plugin_module.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/ppb_buffer_impl.h"
#include "webkit/plugins/ppapi/resource_helper.h"
#include "webkit/plugins/ppapi/resource_tracker.h"

using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_Buffer_API;
using ppapi::thunk::PPB_VideoCapture_API;

namespace webkit {
namespace ppapi {

PPB_VideoCapture_Impl::PPB_VideoCapture_Impl(PP_Instance instance)
    : Resource(instance),
      buffer_count_hint_(0),
      ppp_videocapture_(NULL),
      status_(PP_VIDEO_CAPTURE_STATUS_STOPPED) {
}

PPB_VideoCapture_Impl::~PPB_VideoCapture_Impl() {
  if (platform_video_capture_.get())
    StopCapture();
}

bool PPB_VideoCapture_Impl::Init() {
  PluginInstance* instance = ResourceHelper::GetPluginInstance(this);
  if (!instance)
    return false;
  ppp_videocapture_ = static_cast<const PPP_VideoCapture_Dev*>(
      instance->module()->GetPluginInterface(PPP_VIDEO_CAPTURE_DEV_INTERFACE));
  if (!ppp_videocapture_)
    return false;

  platform_video_capture_.reset(
      instance->delegate()->CreateVideoCapture(this));
  return !!platform_video_capture_.get();
}

PPB_VideoCapture_API* PPB_VideoCapture_Impl::AsPPB_VideoCapture_API() {
  return this;
}

int32_t PPB_VideoCapture_Impl::StartCapture(
    const PP_VideoCaptureDeviceInfo_Dev& requested_info,
    uint32_t buffer_count) {
  switch (status_) {
    case PP_VIDEO_CAPTURE_STATUS_STARTING:
    case PP_VIDEO_CAPTURE_STATUS_STARTED:
    case PP_VIDEO_CAPTURE_STATUS_PAUSED:
    default:
      return PP_ERROR_FAILED;
    case PP_VIDEO_CAPTURE_STATUS_STOPPED:
    case PP_VIDEO_CAPTURE_STATUS_STOPPING:
      break;
  }
  DCHECK(buffers_.empty());

  buffer_count_hint_ = std::min(buffer_count, 1U);
  media::VideoCapture::VideoCaptureCapability capability = {
    requested_info.width,
    requested_info.height,
    requested_info.frames_per_second,
    0,  // ignored.
    media::VideoFrame::I420,
    false,  // ignored
    false  // resolution_fixed
  };
  status_ = PP_VIDEO_CAPTURE_STATUS_STARTING;
  platform_video_capture_->StartCapture(this, capability);
  return PP_OK;
}

int32_t PPB_VideoCapture_Impl::ReuseBuffer(uint32_t buffer) {
  if (buffer >= buffers_.size() || !buffers_[buffer].in_use)
    return PP_ERROR_BADARGUMENT;
  buffers_[buffer].in_use = false;
  return PP_OK;
}

int32_t PPB_VideoCapture_Impl::StopCapture() {
  switch (status_) {
    case PP_VIDEO_CAPTURE_STATUS_STOPPED:
    case PP_VIDEO_CAPTURE_STATUS_STOPPING:
    default:
      return PP_ERROR_FAILED;
    case PP_VIDEO_CAPTURE_STATUS_STARTING:
    case PP_VIDEO_CAPTURE_STATUS_STARTED:
    case PP_VIDEO_CAPTURE_STATUS_PAUSED:
      break;
  }
  ReleaseBuffers();
  status_ = PP_VIDEO_CAPTURE_STATUS_STOPPING;
  platform_video_capture_->StopCapture(this);
  return PP_OK;
}

void PPB_VideoCapture_Impl::OnStarted(media::VideoCapture* capture) {
  switch (status_) {
    case PP_VIDEO_CAPTURE_STATUS_STARTING:
    case PP_VIDEO_CAPTURE_STATUS_PAUSED:
      break;
    case PP_VIDEO_CAPTURE_STATUS_STOPPED:
    case PP_VIDEO_CAPTURE_STATUS_STOPPING:
    case PP_VIDEO_CAPTURE_STATUS_STARTED:
    default:
      return;
  }
  status_ = PP_VIDEO_CAPTURE_STATUS_STARTED;
  SendStatus();
}

void PPB_VideoCapture_Impl::OnStopped(media::VideoCapture* capture) {
  switch (status_) {
    case PP_VIDEO_CAPTURE_STATUS_STOPPING:
      break;
    case PP_VIDEO_CAPTURE_STATUS_STARTING:
    case PP_VIDEO_CAPTURE_STATUS_PAUSED:
    case PP_VIDEO_CAPTURE_STATUS_STOPPED:
    case PP_VIDEO_CAPTURE_STATUS_STARTED:
    default:
      return;
  }
  status_ = PP_VIDEO_CAPTURE_STATUS_STOPPED;
  SendStatus();
}

void PPB_VideoCapture_Impl::OnPaused(media::VideoCapture* capture) {
  switch (status_) {
    case PP_VIDEO_CAPTURE_STATUS_STARTING:
    case PP_VIDEO_CAPTURE_STATUS_STARTED:
      break;
    case PP_VIDEO_CAPTURE_STATUS_STOPPED:
    case PP_VIDEO_CAPTURE_STATUS_STOPPING:
    case PP_VIDEO_CAPTURE_STATUS_PAUSED:
    default:
      return;
  }
  status_ = PP_VIDEO_CAPTURE_STATUS_PAUSED;
  SendStatus();
}

void PPB_VideoCapture_Impl::OnError(media::VideoCapture* capture,
                                    int error_code) {
  // Today, the media layer only sends "1" as an error.
  DCHECK(error_code == 1);
  // It either comes because some error was detected while starting (e.g. 2
  // conflicting "master" resolution), or because the browser failed to start
  // the capture.
  status_ = PP_VIDEO_CAPTURE_STATUS_STOPPED;
  ppp_videocapture_->OnError(pp_instance(), pp_resource(), PP_ERROR_FAILED);
}

void PPB_VideoCapture_Impl::OnBufferReady(
    media::VideoCapture* capture,
    scoped_refptr<media::VideoCapture::VideoFrameBuffer> buffer) {
  DCHECK(buffer.get());
  for (uint32_t i = 0; i < buffers_.size(); ++i) {
    if (!buffers_[i].in_use) {
      // TODO(piman): it looks like stride isn't actually used/filled.
      DCHECK(buffer->stride == 0);
      size_t size = std::min(static_cast<size_t>(buffers_[i].buffer->size()),
          buffer->buffer_size);
      memcpy(buffers_[i].data, buffer->memory_pointer, size);
      platform_video_capture_->FeedBuffer(buffer);
      ppp_videocapture_->OnBufferReady(pp_instance(), pp_resource(), i);
      return;
    }
  }
  // TODO(piman): signal dropped buffers ?
  platform_video_capture_->FeedBuffer(buffer);
}

void PPB_VideoCapture_Impl::OnDeviceInfoReceived(
    media::VideoCapture* capture,
    const media::VideoCaptureParams& device_info) {
  PP_VideoCaptureDeviceInfo_Dev info = {
    device_info.width,
    device_info.height,
    device_info.frame_per_second
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
      ResourceTracker::Get()->ReleaseResource(resources[i]);
      break;
    }
    buffers_.push_back(info);
  }

  if (buffers_.empty()) {
    // We couldn't allocate/map buffers at all. Send an error and stop the
    // capture.
    ppp_videocapture_->OnError(pp_instance(), pp_resource(), PP_ERROR_NOMEMORY);
    status_ = PP_VIDEO_CAPTURE_STATUS_STOPPING;
    platform_video_capture_->StopCapture(this);
    return;
  }

  ppp_videocapture_->OnDeviceInfo(pp_instance(), pp_resource(), &info,
                                  buffers_.size(), resources.get());
}

void PPB_VideoCapture_Impl::ReleaseBuffers() {
  ResourceTracker *tracker = ResourceTracker::Get();
  for (size_t i = 0; i < buffers_.size(); ++i) {
    buffers_[i].buffer->Unmap();
    tracker->ReleaseResource(buffers_[i].buffer->pp_resource());
  }
  buffers_.clear();
}

void PPB_VideoCapture_Impl::SendStatus() {
  ppp_videocapture_->OnStatus(pp_instance(), pp_resource(), status_);
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

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/ppb_video_capture_shared.h"

#include "base/logging.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/shared_impl/ppb_device_ref_shared.h"
#include "ppapi/shared_impl/ppb_resource_array_shared.h"

namespace ppapi {

PPB_VideoCapture_Shared::PPB_VideoCapture_Shared(PP_Instance instance)
    : Resource(OBJECT_IS_IMPL, instance),
      open_state_(BEFORE_OPEN),
      status_(PP_VIDEO_CAPTURE_STATUS_STOPPED),
      devices_(NULL),
      resource_object_type_(OBJECT_IS_IMPL) {
}

PPB_VideoCapture_Shared::PPB_VideoCapture_Shared(
    const HostResource& host_resource)
    : Resource(OBJECT_IS_PROXY, host_resource),
      open_state_(BEFORE_OPEN),
      status_(PP_VIDEO_CAPTURE_STATUS_STOPPED),
      devices_(NULL),
      resource_object_type_(OBJECT_IS_PROXY) {
}

PPB_VideoCapture_Shared::~PPB_VideoCapture_Shared() {
}

thunk::PPB_VideoCapture_API* PPB_VideoCapture_Shared::AsPPB_VideoCapture_API() {
  return this;
}

int32_t PPB_VideoCapture_Shared::EnumerateDevices(
    PP_Resource* devices,
    scoped_refptr<TrackedCallback> callback) {
  if (TrackedCallback::IsPending(enumerate_devices_callback_))
    return PP_ERROR_INPROGRESS;

  return InternalEnumerateDevices(devices, callback);
}

int32_t PPB_VideoCapture_Shared::Open(
    const std::string& device_id,
    const PP_VideoCaptureDeviceInfo_Dev& requested_info,
    uint32_t buffer_count,
    scoped_refptr<TrackedCallback> callback) {
  if (open_state_ != BEFORE_OPEN)
    return PP_ERROR_FAILED;

  if (TrackedCallback::IsPending(open_callback_))
    return PP_ERROR_INPROGRESS;

  return InternalOpen(device_id, requested_info, buffer_count, callback);
}

int32_t PPB_VideoCapture_Shared::StartCapture() {
  if (open_state_ != OPENED ||
      !SetStatus(PP_VIDEO_CAPTURE_STATUS_STARTING, false)) {
    return PP_ERROR_FAILED;
  }

  return InternalStartCapture();
}

int32_t PPB_VideoCapture_Shared::ReuseBuffer(uint32_t buffer) {
  return InternalReuseBuffer(buffer);
}

int32_t PPB_VideoCapture_Shared::StopCapture() {
  if (open_state_ != OPENED ||
      !SetStatus(PP_VIDEO_CAPTURE_STATUS_STOPPING, false)) {
    return PP_ERROR_FAILED;
  }

  return InternalStopCapture();
}

void PPB_VideoCapture_Shared::Close() {
  if (open_state_ == CLOSED)
    return;

  InternalClose();
  open_state_ = CLOSED;

  if (TrackedCallback::IsPending(open_callback_))
    open_callback_->PostAbort();
}

int32_t PPB_VideoCapture_Shared::StartCapture0_1(
    const PP_VideoCaptureDeviceInfo_Dev& requested_info,
    uint32_t buffer_count) {
  if (open_state_ == BEFORE_OPEN) {
    if (TrackedCallback::IsPending(open_callback_))
      return PP_ERROR_FAILED;
    open_state_ = OPENED;
  } else if (open_state_ == CLOSED) {
    return PP_ERROR_FAILED;
  }

  if (!SetStatus(PP_VIDEO_CAPTURE_STATUS_STARTING, false))
    return PP_ERROR_FAILED;

  return InternalStartCapture0_1(requested_info, buffer_count);
}

const std::vector<DeviceRefData>& PPB_VideoCapture_Shared::GetDeviceRefData(
    ) const {
  return InternalGetDeviceRefData();
}

void PPB_VideoCapture_Shared::OnEnumerateDevicesComplete(
    int32_t result,
    const std::vector<DeviceRefData>& devices) {
  DCHECK(TrackedCallback::IsPending(enumerate_devices_callback_));

  if (result == PP_OK && devices_) {
    *devices_ = PPB_DeviceRef_Shared::CreateResourceArray(
        resource_object_type_, pp_instance(), devices);
  }
  devices_ = NULL;

  TrackedCallback::ClearAndRun(&enumerate_devices_callback_, result);
}

void PPB_VideoCapture_Shared::OnOpenComplete(int32_t result) {
  if (open_state_ == BEFORE_OPEN && result == PP_OK)
    open_state_ = OPENED;

  // The callback may have been aborted by Close(), or the open operation is
  // completed synchronously.
  if (TrackedCallback::IsPending(open_callback_))
    TrackedCallback::ClearAndRun(&open_callback_, result);
}

bool PPB_VideoCapture_Shared::SetStatus(PP_VideoCaptureStatus_Dev status,
                                        bool forced) {
  if (!forced) {
    switch (status) {
      case PP_VIDEO_CAPTURE_STATUS_STOPPED:
        if (status_ != PP_VIDEO_CAPTURE_STATUS_STOPPING)
          return false;
        break;
      case PP_VIDEO_CAPTURE_STATUS_STARTING:
        if (status_ != PP_VIDEO_CAPTURE_STATUS_STOPPED)
          return false;
        break;
      case PP_VIDEO_CAPTURE_STATUS_STARTED:
        switch (status_) {
          case PP_VIDEO_CAPTURE_STATUS_STARTING:
          case PP_VIDEO_CAPTURE_STATUS_PAUSED:
            break;
          default:
            return false;
        }
        break;
      case PP_VIDEO_CAPTURE_STATUS_PAUSED:
        switch (status_) {
          case PP_VIDEO_CAPTURE_STATUS_STARTING:
          case PP_VIDEO_CAPTURE_STATUS_STARTED:
            break;
          default:
            return false;
        }
        break;
      case PP_VIDEO_CAPTURE_STATUS_STOPPING:
        switch (status_) {
          case PP_VIDEO_CAPTURE_STATUS_STARTING:
          case PP_VIDEO_CAPTURE_STATUS_STARTED:
          case PP_VIDEO_CAPTURE_STATUS_PAUSED:
            break;
          default:
            return false;
        }
        break;
    }
  }

  status_ = status;
  return true;
}

}  // namespace ppapi

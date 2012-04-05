// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/video_capture_dev.h"

#include "ppapi/c/dev/ppb_video_capture_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/dev/resource_array_dev.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_VideoCapture_Dev_0_2>() {
  return PPB_VIDEOCAPTURE_DEV_INTERFACE_0_2;
}

template <> const char* interface_name<PPB_VideoCapture_Dev_0_1>() {
  return PPB_VIDEOCAPTURE_DEV_INTERFACE_0_1;
}

}  // namespace

VideoCapture_Dev::VideoCapture_Dev(const InstanceHandle& instance)
    : requested_info_(),
      buffer_count_(0) {
  if (has_interface<PPB_VideoCapture_Dev_0_2>()) {
    PassRefFromConstructor(get_interface<PPB_VideoCapture_Dev_0_2>()->Create(
        instance.pp_instance()));
  } else if (has_interface<PPB_VideoCapture_Dev_0_1>()) {
    PassRefFromConstructor(get_interface<PPB_VideoCapture_Dev_0_1>()->Create(
        instance.pp_instance()));
  }
}

VideoCapture_Dev::VideoCapture_Dev(PP_Resource resource)
    : Resource(resource),
      requested_info_(),
      buffer_count_(0) {
}

VideoCapture_Dev::~VideoCapture_Dev() {
}

// static
bool VideoCapture_Dev::IsAvailable() {
  return has_interface<PPB_VideoCapture_Dev_0_2>() ||
         has_interface<PPB_VideoCapture_Dev_0_1>();
}

int32_t VideoCapture_Dev::EnumerateDevices(
    const CompletionCallbackWithOutput<std::vector<DeviceRef_Dev> >& callback) {
  if (!has_interface<PPB_VideoCapture_Dev_0_2>())
    return callback.MayForce(PP_ERROR_NOINTERFACE);
  if (!callback.pp_completion_callback().func)
    return callback.MayForce(PP_ERROR_BLOCKS_MAIN_THREAD);

  // ArrayOutputCallbackConverter is responsible to delete it.
  ResourceArray_Dev::ArrayOutputCallbackData* data =
      new ResourceArray_Dev::ArrayOutputCallbackData(
          callback.output(), callback.pp_completion_callback());
  return get_interface<PPB_VideoCapture_Dev_0_2>()->EnumerateDevices(
      pp_resource(), &data->resource_array_output,
      PP_MakeCompletionCallback(
          &ResourceArray_Dev::ArrayOutputCallbackConverter, data));
}

int32_t VideoCapture_Dev::Open(
    const DeviceRef_Dev& device_ref,
    const PP_VideoCaptureDeviceInfo_Dev& requested_info,
    uint32_t buffer_count,
    const CompletionCallback& callback) {
  if (has_interface<PPB_VideoCapture_Dev_0_2>()) {
    return get_interface<PPB_VideoCapture_Dev_0_2>()->Open(
        pp_resource(), device_ref.pp_resource(), &requested_info, buffer_count,
        callback.pp_completion_callback());
  }

  if (has_interface<PPB_VideoCapture_Dev_0_1>()) {
    if (device_ref.is_null()) {
      requested_info_ = requested_info;
      buffer_count_ = buffer_count;
      return callback.MayForce(PP_OK);
    }
    return callback.MayForce(PP_ERROR_NOTSUPPORTED);
  }

  return callback.MayForce(PP_ERROR_NOINTERFACE);
}

int32_t VideoCapture_Dev::StartCapture() {
  if (has_interface<PPB_VideoCapture_Dev_0_2>()) {
    return get_interface<PPB_VideoCapture_Dev_0_2>()->StartCapture(
        pp_resource());
  }

  if (has_interface<PPB_VideoCapture_Dev_0_1>()) {
    return get_interface<PPB_VideoCapture_Dev_0_1>()->StartCapture(
        pp_resource(), &requested_info_, buffer_count_);
  }

  return PP_ERROR_NOINTERFACE;
}

int32_t VideoCapture_Dev::ReuseBuffer(uint32_t buffer) {
  if (has_interface<PPB_VideoCapture_Dev_0_2>()) {
    return get_interface<PPB_VideoCapture_Dev_0_2>()->ReuseBuffer(pp_resource(),
                                                                  buffer);
  }

  if (has_interface<PPB_VideoCapture_Dev_0_1>()) {
    return get_interface<PPB_VideoCapture_Dev_0_1>()->ReuseBuffer(pp_resource(),
                                                                  buffer);
  }

  return PP_ERROR_NOINTERFACE;
}

int32_t VideoCapture_Dev::StopCapture() {
  if (has_interface<PPB_VideoCapture_Dev_0_2>()) {
    return get_interface<PPB_VideoCapture_Dev_0_2>()->StopCapture(
        pp_resource());
  }

  if (has_interface<PPB_VideoCapture_Dev_0_1>()) {
    return get_interface<PPB_VideoCapture_Dev_0_1>()->StopCapture(
        pp_resource());
  }

  return PP_ERROR_NOINTERFACE;
}

void VideoCapture_Dev::Close() {
  if (has_interface<PPB_VideoCapture_Dev_0_2>())
    get_interface<PPB_VideoCapture_Dev_0_2>()->Close(pp_resource());
}

}  // namespace pp

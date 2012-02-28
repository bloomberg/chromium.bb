// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/video_capture_dev.h"

#include "ppapi/c/dev/ppb_video_capture_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/dev/device_ref_dev.h"
#include "ppapi/cpp/dev/resource_array_dev.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module.h"
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

struct VideoCapture_Dev::EnumerateDevicesState {
  EnumerateDevicesState(std::vector<DeviceRef_Dev>* in_devices,
                        const CompletionCallback& in_callback,
                        VideoCapture_Dev* in_video_capture)
      : devices_resource(0),
        devices(in_devices),
        callback(in_callback),
        video_capture(in_video_capture) {
  }

  PP_Resource devices_resource;
  std::vector<DeviceRef_Dev>* devices;
  CompletionCallback callback;
  VideoCapture_Dev* video_capture;
};

VideoCapture_Dev::VideoCapture_Dev(const InstanceHandle& instance)
    : enum_state_(NULL),
      requested_info_(),
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
      enum_state_(NULL),
      requested_info_(),
      buffer_count_(0) {
}

VideoCapture_Dev::VideoCapture_Dev(const VideoCapture_Dev& other)
    : Resource(other),
      enum_state_(NULL),
      requested_info_(other.requested_info_),
      buffer_count_(other.buffer_count_) {
}

VideoCapture_Dev::~VideoCapture_Dev() {
  AbortEnumerateDevices();
}

VideoCapture_Dev& VideoCapture_Dev::operator=(
    const VideoCapture_Dev& other) {
  AbortEnumerateDevices();

  Resource::operator=(other);
  requested_info_ = other.requested_info_;
  buffer_count_ = other.buffer_count_;
  return *this;
}

// static
bool VideoCapture_Dev::IsAvailable() {
  return has_interface<PPB_VideoCapture_Dev_0_2>() ||
         has_interface<PPB_VideoCapture_Dev_0_1>();
}

int32_t VideoCapture_Dev::EnumerateDevices(std::vector<DeviceRef_Dev>* devices,
                                           const CompletionCallback& callback) {
  if (!has_interface<PPB_VideoCapture_Dev_0_2>())
    return callback.MayForce(PP_ERROR_NOINTERFACE);
  if (!devices)
    return callback.MayForce(PP_ERROR_BADARGUMENT);
  if (!callback.pp_completion_callback().func)
    return callback.MayForce(PP_ERROR_BLOCKS_MAIN_THREAD);
  if (enum_state_)
    return callback.MayForce(PP_ERROR_INPROGRESS);

  // It will be deleted in OnEnumerateDevicesComplete().
  enum_state_ = new EnumerateDevicesState(devices, callback, this);
  return get_interface<PPB_VideoCapture_Dev_0_2>()->EnumerateDevices(
      pp_resource(), &enum_state_->devices_resource,
      PP_MakeCompletionCallback(&VideoCapture_Dev::OnEnumerateDevicesComplete,
                                enum_state_));
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

void VideoCapture_Dev::AbortEnumerateDevices() {
  if (enum_state_) {
    enum_state_->devices = NULL;
    Module::Get()->core()->CallOnMainThread(0, enum_state_->callback,
                                            PP_ERROR_ABORTED);
    enum_state_->video_capture = NULL;
    enum_state_ = NULL;
  }
}

// static
void VideoCapture_Dev::OnEnumerateDevicesComplete(void* user_data,
                                                  int32_t result) {
  EnumerateDevicesState* enum_state =
      static_cast<EnumerateDevicesState*>(user_data);

  bool need_to_callback = !!enum_state->video_capture;

  if (result == PP_OK) {
    // It will take care of releasing the reference.
    ResourceArray_Dev resources(pp::PASS_REF, enum_state->devices_resource);

    if (need_to_callback) {
      enum_state->devices->clear();
      for (uint32_t index = 0; index < resources.size(); ++index) {
        DeviceRef_Dev device(resources[index]);
        enum_state->devices->push_back(device);
      }
    }
  }

  if (need_to_callback) {
    enum_state->video_capture->enum_state_ = NULL;
    enum_state->callback.Run(result);
  }

  delete enum_state;
}

}  // namespace pp

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/video_capture_dev.h"

#include "ppapi/c/dev/ppb_video_capture_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_VideoCapture_Dev>() {
  return PPB_VIDEOCAPTURE_DEV_INTERFACE;
}

}  // namespace

VideoCapture_Dev::VideoCapture_Dev(const Instance& instance) {
  if (!has_interface<PPB_VideoCapture_Dev>())
    return;
  PassRefFromConstructor(get_interface<PPB_VideoCapture_Dev>()->Create(
      instance.pp_instance()));
}

VideoCapture_Dev::VideoCapture_Dev(PP_Resource resource) : Resource(resource) {
}

VideoCapture_Dev::VideoCapture_Dev(const VideoCapture_Dev& other)
    : Resource(other) {
}

// static
bool VideoCapture_Dev::IsAvailable() {
  return has_interface<PPB_VideoCapture_Dev>();
}

int32_t VideoCapture_Dev::StartCapture(
    const PP_VideoCaptureDeviceInfo_Dev& requested_info,
    uint32_t buffer_count) {
  if (!has_interface<PPB_VideoCapture_Dev>())
    return PP_ERROR_FAILED;
  return get_interface<PPB_VideoCapture_Dev>()->StartCapture(
      pp_resource(), &requested_info, buffer_count);
}

int32_t VideoCapture_Dev::ReuseBuffer(uint32_t buffer) {
  if (!has_interface<PPB_VideoCapture_Dev>())
    return PP_ERROR_FAILED;
  return get_interface<PPB_VideoCapture_Dev>()->ReuseBuffer(
      pp_resource(), buffer);
}

int32_t VideoCapture_Dev::StopCapture(){
  if (!has_interface<PPB_VideoCapture_Dev>())
    return PP_ERROR_FAILED;
  return get_interface<PPB_VideoCapture_Dev>()->StopCapture(pp_resource());
}

}  // namespace pp

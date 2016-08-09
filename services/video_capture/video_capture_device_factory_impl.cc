// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "services/video_capture/video_capture_device_factory_impl.h"

namespace video_capture {

void VideoCaptureDeviceFactoryImpl::EnumerateDeviceDescriptors(
    const EnumerateDeviceDescriptorsCallback& callback) {
  NOTIMPLEMENTED();
}

void VideoCaptureDeviceFactoryImpl::GetSupportedFormats(
    mojom::VideoCaptureDeviceDescriptorPtr device_descriptor,
    const GetSupportedFormatsCallback& callback) {
  NOTIMPLEMENTED();
}

void VideoCaptureDeviceFactoryImpl::CreateDevice(
    mojom::VideoCaptureDeviceDescriptorPtr device_descriptor,
    mojom::VideoCaptureDeviceRequest device_request) {
  NOTIMPLEMENTED();
}

}  // namespace video_capture

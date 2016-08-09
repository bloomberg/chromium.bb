// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_VIDEO_CAPTURE_DEVICE_FACTORY_IMPL_H_
#define SERVICES_VIDEO_CAPTURE_VIDEO_CAPTURE_DEVICE_FACTORY_IMPL_H_

#include "services/video_capture/public/interfaces/video_capture_device_factory.mojom.h"

namespace video_capture {

// Implementation of the VideoCaptureDeviceFactory Mojo interface.
class VideoCaptureDeviceFactoryImpl : public mojom::VideoCaptureDeviceFactory {
 public:
  // mojom::VideoCaptureDeviceFactory:
  void EnumerateDeviceDescriptors(
      const EnumerateDeviceDescriptorsCallback& callback) override;
  void GetSupportedFormats(
      mojom::VideoCaptureDeviceDescriptorPtr device_descriptor,
      const GetSupportedFormatsCallback& callback) override;
  void CreateDevice(mojom::VideoCaptureDeviceDescriptorPtr device_descriptor,
                    mojom::VideoCaptureDeviceRequest device_request) override;
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_VIDEO_CAPTURE_DEVICE_FACTORY_IMPL_H_

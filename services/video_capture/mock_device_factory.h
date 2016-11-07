// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_MOCK_DEVICE_FACTORY_H_
#define SERVICES_VIDEO_CAPTURE_MOCK_DEVICE_FACTORY_H_

#include <map>

#include "media/capture/video/video_capture_device_factory.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/video_capture/public/interfaces/mock_video_capture_device.mojom.h"
#include "services/video_capture/public/interfaces/video_capture_device_descriptor.mojom.h"
#include "services/video_capture/video_capture_device_proxy_impl.h"

namespace video_capture {

// Implementation of media::VideoCaptureDeviceFactory that allows clients to
// add mock devices of type mojom::VideoCaptureDeviceDescriptorPtr.
class MockDeviceFactory : public media::VideoCaptureDeviceFactory {
 public:
  MockDeviceFactory();
  ~MockDeviceFactory() override;

  void AddMockDevice(mojom::MockVideoCaptureDevicePtr device,
                     const media::VideoCaptureDeviceDescriptor& descriptor);

  // media::VideoCaptureDeviceFactory:
  std::unique_ptr<media::VideoCaptureDevice> CreateDevice(
      const media::VideoCaptureDeviceDescriptor& device_descriptor) override;
  void GetDeviceDescriptors(
      media::VideoCaptureDeviceDescriptors* device_descriptors) override;
  void GetSupportedFormats(
      const media::VideoCaptureDeviceDescriptor& device_descriptor,
      media::VideoCaptureFormats* supported_formats) override;

 private:
  std::map<media::VideoCaptureDeviceDescriptor,
           mojom::MockVideoCaptureDevicePtr>
      devices_;
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_MOCK_DEVICE_FACTORY_H_

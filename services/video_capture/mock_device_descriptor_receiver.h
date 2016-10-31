// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_MOCK_DEVICE_DESCRIPTOR_RECEIVER_H_
#define SERVICES_VIDEO_CAPTURE_MOCK_DEVICE_DESCRIPTOR_RECEIVER_H_

#include "services/video_capture/public/interfaces/video_capture_device_factory.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace video_capture {

// Mock for receiving device descriptors from a call to
// mojom::VideoCaptureDeviceFactory::EnumerateDeviceDescriptors().
class MockDeviceDescriptorReceiver {
 public:
  MockDeviceDescriptorReceiver();
  ~MockDeviceDescriptorReceiver();

  // Use forwarding method to work around gmock not supporting move-only types.
  void HandleEnumerateDeviceDescriptorsCallback(
      std::vector<mojom::VideoCaptureDeviceDescriptorPtr> descriptors);

  MOCK_METHOD1(
      OnEnumerateDeviceDescriptorsCallback,
      void(const std::vector<mojom::VideoCaptureDeviceDescriptorPtr>&));
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_MOCK_DEVICE_DESCRIPTOR_RECEIVER_H_

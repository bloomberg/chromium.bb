// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_FAKE_DEVICE_TEST_H_
#define SERVICES_VIDEO_CAPTURE_FAKE_DEVICE_TEST_H_

#include "services/video_capture/fake_device_descriptor_test.h"

namespace video_capture {

// Test fixture that creates a proxy to the fake device provided by the fake
// device factory.
class FakeDeviceTest : public FakeDeviceDescriptorTest {
 public:
  FakeDeviceTest();
  ~FakeDeviceTest() override;

  void SetUp() override;

 protected:
  mojom::VideoCaptureDeviceProxyPtr fake_device_proxy_;
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_FAKE_DEVICE_TEST_H_

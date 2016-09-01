// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_VIDEO_CAPTURE_SERVICE_TEST_H_
#define SERVICES_VIDEO_CAPTURE_VIDEO_CAPTURE_SERVICE_TEST_H_

#include "services/shell/public/cpp/service_test.h"
#include "services/video_capture/mock_device_descriptor_receiver.h"
#include "services/video_capture/public/interfaces/video_capture_service.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace video_capture {

// Basic test fixture that sets up a connection to the fake device factory.
class VideoCaptureServiceTest : public shell::test::ServiceTest {
 public:
  VideoCaptureServiceTest();
  ~VideoCaptureServiceTest() override;

  void SetUp() override;

 protected:
  mojom::VideoCaptureServicePtr service_;
  mojom::VideoCaptureDeviceFactoryPtr factory_;
  MockDeviceDescriptorReceiver descriptor_receiver_;
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_VIDEO_CAPTURE_SERVICE_TEST_H_

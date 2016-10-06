// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/video_capture_service_test.h"

namespace video_capture {

VideoCaptureServiceTest::VideoCaptureServiceTest()
    : shell::test::ServiceTest("exe:video_capture_unittests") {}

VideoCaptureServiceTest::~VideoCaptureServiceTest() = default;

void VideoCaptureServiceTest::SetUp() {
  ServiceTest::SetUp();
  connector()->ConnectToInterface("service:video_capture", &service_);
  service_->ConnectToFakeDeviceFactory(mojo::GetProxy(&factory_));
}

}  // namespace video_capture

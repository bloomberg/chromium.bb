// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/mock_device_test.h"

namespace video_capture {

MockDeviceTest::MockDeviceTest()
    : service_manager::test::ServiceTest("exe:video_capture_unittests") {}

MockDeviceTest::~MockDeviceTest() =
    default;

void MockDeviceTest::SetUp() {
  ServiceTest::SetUp();
  connector()->ConnectToInterface("service:video_capture", &service_);
  service_->ConnectToMockDeviceFactory(mojo::GetProxy(&factory_));

  // Set up a mock device and add it to the factory
  mock_device_ = base::MakeUnique<MockVideoCaptureDeviceImpl>(
      mojo::GetProxy(&mock_device_proxy_));
  auto mock_descriptor = mojom::VideoCaptureDeviceDescriptor::New();
  mock_descriptor->device_id = "MockDeviceId";
  ASSERT_TRUE(service_->AddDeviceToMockFactory(std::move(mock_device_proxy_),
                                               mock_descriptor->Clone()));

  // Obtain the mock device from the factory
  factory_->CreateDeviceProxy(
      mock_descriptor->Clone(), mojo::GetProxy(&device_proxy_),
      base::Bind([](mojom::DeviceAccessResultCode result_code) {}));

  requested_settings_.format.frame_size = gfx::Size(800, 600);
  requested_settings_.format.frame_rate = 15;
  requested_settings_.resolution_change_policy =
      media::RESOLUTION_POLICY_FIXED_RESOLUTION;
  requested_settings_.power_line_frequency =
      media::PowerLineFrequency::FREQUENCY_DEFAULT;

  mock_receiver_ = base::MakeUnique<MockVideoFrameReceiver>(
      mojo::GetProxy(&mock_receiver_proxy_));
}

}  // namespace video_capture

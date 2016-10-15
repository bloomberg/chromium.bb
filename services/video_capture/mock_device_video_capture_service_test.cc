// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/mock_device_video_capture_service_test.h"

namespace video_capture {

MockDeviceVideoCaptureServiceTest::MockDeviceVideoCaptureServiceTest()
    : service_manager::test::ServiceTest("exe:video_capture_unittests") {}

MockDeviceVideoCaptureServiceTest::~MockDeviceVideoCaptureServiceTest() =
    default;

void MockDeviceVideoCaptureServiceTest::SetUp() {
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

  requested_format_.frame_size = gfx::Size(800, 600);
  requested_format_.frame_rate = 15;
  requested_format_.pixel_format = media::PIXEL_FORMAT_I420;
  requested_format_.pixel_storage = media::PIXEL_STORAGE_CPU;
  mock_receiver_ = base::MakeUnique<MockVideoFrameReceiver>(
      mojo::GetProxy(&mock_receiver_proxy_));
}

}  // namespace video_capture

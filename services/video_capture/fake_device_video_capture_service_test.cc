// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/fake_device_video_capture_service_test.h"

#include "base/run_loop.h"

using testing::_;
using testing::Invoke;

namespace video_capture {

FakeDeviceVideoCaptureServiceTest::FakeDeviceVideoCaptureServiceTest()
    : VideoCaptureServiceTest() {}

FakeDeviceVideoCaptureServiceTest::~FakeDeviceVideoCaptureServiceTest() =
    default;

void FakeDeviceVideoCaptureServiceTest::SetUp() {
  VideoCaptureServiceTest::SetUp();

  base::RunLoop wait_loop;
  mojom::VideoCaptureDeviceDescriptorPtr fake_device_descriptor;
  EXPECT_CALL(descriptor_receiver_, OnEnumerateDeviceDescriptorsCallback(_))
      .WillOnce(Invoke([&wait_loop, &fake_device_descriptor](
          const std::vector<mojom::VideoCaptureDeviceDescriptorPtr>&
              descriptors) {
        fake_device_descriptor = descriptors[0].Clone();
        wait_loop.Quit();
      }));
  factory_->EnumerateDeviceDescriptors(base::Bind(
      &MockDeviceDescriptorReceiver::HandleEnumerateDeviceDescriptorsCallback,
      base::Unretained(&descriptor_receiver_)));
  wait_loop.Run();

  factory_->CreateDeviceProxy(
      std::move(fake_device_descriptor), mojo::GetProxy(&fake_device_proxy_),
      base::Bind([](mojom::DeviceAccessResultCode result_code) {
        ASSERT_EQ(mojom::DeviceAccessResultCode::SUCCESS, result_code);
      }));
}

}  // namespace video_capture

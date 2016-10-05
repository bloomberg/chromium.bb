// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "services/video_capture/fake_device_descriptor_test.h"
#include "services/video_capture/mock_video_capture_device_client.h"

using testing::_;
using testing::InvokeWithoutArgs;

namespace video_capture {

class MockCreateDeviceProxyCallback {
 public:
  MOCK_METHOD1(Run, void(mojom::DeviceAccessResultCode result_code));
};

// Tests that when requesting a second proxy for a device without closing the
// first one, the service revokes access to the first one by closing the
// connection.
TEST_F(FakeDeviceDescriptorTest, AccessIsRevokedOnSecondAccess) {
  mojom::VideoCaptureDeviceProxyPtr device_proxy_1;
  bool device_access_1_revoked = false;
  MockCreateDeviceProxyCallback create_device_proxy_callback_1;
  EXPECT_CALL(create_device_proxy_callback_1,
              Run(mojom::DeviceAccessResultCode::SUCCESS))
      .Times(1);
  factory_->CreateDeviceProxy(
      fake_device_descriptor_->Clone(), mojo::GetProxy(&device_proxy_1),
      base::Bind(&MockCreateDeviceProxyCallback::Run,
                 base::Unretained(&create_device_proxy_callback_1)));
  device_proxy_1.set_connection_error_handler(
      base::Bind([](bool* access_revoked) { *access_revoked = true; },
                 &device_access_1_revoked));

  base::RunLoop wait_loop;
  mojom::VideoCaptureDeviceProxyPtr device_proxy_2;
  bool device_access_2_revoked = false;
  MockCreateDeviceProxyCallback create_device_proxy_callback_2;
  EXPECT_CALL(create_device_proxy_callback_2,
              Run(mojom::DeviceAccessResultCode::SUCCESS))
      .Times(1)
      .WillOnce(InvokeWithoutArgs([&wait_loop]() { wait_loop.Quit(); }));
  factory_->CreateDeviceProxy(
      fake_device_descriptor_->Clone(), mojo::GetProxy(&device_proxy_2),
      base::Bind(&MockCreateDeviceProxyCallback::Run,
                 base::Unretained(&create_device_proxy_callback_2)));
  device_proxy_2.set_connection_error_handler(
      base::Bind([](bool* access_revoked) { *access_revoked = true; },
                 &device_access_2_revoked));
  wait_loop.Run();
  ASSERT_TRUE(device_access_1_revoked);
  ASSERT_FALSE(device_access_2_revoked);
}

// Tests that a second proxy requested for a device can be used successfully.
TEST_F(FakeDeviceDescriptorTest, CanUseSecondRequestedProxy) {
  mojom::VideoCaptureDeviceProxyPtr device_proxy_1;
  factory_->CreateDeviceProxy(
      fake_device_descriptor_->Clone(), mojo::GetProxy(&device_proxy_1),
      base::Bind([](mojom::DeviceAccessResultCode result_code) {}));

  base::RunLoop wait_loop;
  mojom::VideoCaptureDeviceProxyPtr device_proxy_2;
  factory_->CreateDeviceProxy(
      fake_device_descriptor_->Clone(), mojo::GetProxy(&device_proxy_2),
      base::Bind(
          [](base::RunLoop* wait_loop,
             mojom::DeviceAccessResultCode result_code) { wait_loop->Quit(); },
          &wait_loop));
  wait_loop.Run();

  media::VideoCaptureFormat arbitrary_requested_format;
  arbitrary_requested_format.frame_size.SetSize(640, 480);
  arbitrary_requested_format.frame_rate = 15;
  arbitrary_requested_format.pixel_format = media::PIXEL_FORMAT_I420;
  arbitrary_requested_format.pixel_storage = media::PIXEL_STORAGE_CPU;

  base::RunLoop wait_loop_2;
  mojom::VideoCaptureDeviceClientPtr client_proxy;
  MockVideoCaptureDeviceClient client(mojo::GetProxy(&client_proxy));
  EXPECT_CALL(client, OnFrameAvailablePtr(_))
      .WillRepeatedly(
          InvokeWithoutArgs([&wait_loop_2]() { wait_loop_2.Quit(); }));

  device_proxy_2->Start(arbitrary_requested_format,
                        mojom::ResolutionChangePolicy::FIXED_RESOLUTION,
                        mojom::PowerLineFrequency::DEFAULT,
                        std::move(client_proxy));
  wait_loop_2.Run();
}

}  // namespace video_capture

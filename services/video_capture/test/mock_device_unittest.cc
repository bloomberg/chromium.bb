// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "services/video_capture/test/mock_device_test.h"

using testing::_;
using testing::Invoke;

namespace video_capture {

// This alias ensures test output is easily attributed to this service's tests.
// TODO(rockot/chfremer): Consider just renaming the type.
using MockVideoCaptureDeviceTest = MockDeviceTest;

// Tests that the service stops the capture device when the client closes the
// connection to the device proxy.
TEST_F(MockVideoCaptureDeviceTest,
       DISABLED_DeviceIsStoppedWhenDiscardingDeviceProxy) {
  base::RunLoop wait_loop;

  // The mock device must hold on to the device client that is passed to it.
  std::unique_ptr<media::VideoCaptureDevice::Client> device_client;
  EXPECT_CALL(mock_device_, DoAllocateAndStart(_, _))
      .WillOnce(Invoke([&device_client](
          const media::VideoCaptureParams& params,
          std::unique_ptr<media::VideoCaptureDevice::Client>* client) {
        device_client.reset(client->release());
      }));
  EXPECT_CALL(mock_device_, StopAndDeAllocate())
      .WillOnce(Invoke([&wait_loop]() { wait_loop.Quit(); }));

  device_proxy_->Start(requested_settings_, std::move(mock_receiver_proxy_));
  device_proxy_.reset();

  wait_loop.Run();
}

// Tests that the service stops the capture device when the client closes the
// connection to the client proxy it provided to the service.
TEST_F(MockVideoCaptureDeviceTest,
       DISABLED_DeviceIsStoppedWhenDiscardingDeviceClient) {
  base::RunLoop wait_loop;

  // The mock device must hold on to the device client that is passed to it.
  std::unique_ptr<media::VideoCaptureDevice::Client> device_client;
  EXPECT_CALL(mock_device_, DoAllocateAndStart(_, _))
      .WillOnce(Invoke([&device_client](
          const media::VideoCaptureParams& params,
          std::unique_ptr<media::VideoCaptureDevice::Client>* client) {
        device_client.reset(client->release());
      }));
  EXPECT_CALL(mock_device_, StopAndDeAllocate())
      .WillOnce(Invoke([&wait_loop]() { wait_loop.Quit(); }));

  device_proxy_->Start(requested_settings_, std::move(mock_receiver_proxy_));
  mock_receiver_.reset();

  wait_loop.Run();
}

}  // namespace video_capture

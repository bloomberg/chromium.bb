// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "services/video_capture/public/interfaces/device_factory.mojom.h"
#include "services/video_capture/test/service_test.h"

using testing::Exactly;
using testing::_;
using testing::Invoke;
using testing::InvokeWithoutArgs;

namespace video_capture {

// This alias ensures test output is easily attributed to this service's tests.
// TODO(rockot/chfremer): Consider just renaming the type.
using VideoCaptureServiceTest = ServiceTest;

// Tests that an answer arrives from the service when calling
// GetDeviceInfos().
TEST_F(VideoCaptureServiceTest, DISABLED_GetDeviceInfosCallbackArrives) {
  base::RunLoop wait_loop;
  EXPECT_CALL(device_info_receiver_, Run(_))
      .Times(Exactly(1))
      .WillOnce(InvokeWithoutArgs([&wait_loop]() { wait_loop.Quit(); }));

  factory_->GetDeviceInfos(device_info_receiver_.Get());
  wait_loop.Run();
}

TEST_F(VideoCaptureServiceTest, DISABLED_FakeDeviceFactoryEnumeratesOneDevice) {
  base::RunLoop wait_loop;
  size_t num_devices_enumerated = 0;
  EXPECT_CALL(device_info_receiver_, Run(_))
      .Times(Exactly(1))
      .WillOnce(
          Invoke([&wait_loop, &num_devices_enumerated](
                     const std::vector<media::VideoCaptureDeviceInfo>& infos) {
            num_devices_enumerated = infos.size();
            wait_loop.Quit();
          }));

  factory_->GetDeviceInfos(device_info_receiver_.Get());
  wait_loop.Run();
  ASSERT_EQ(1u, num_devices_enumerated);
}

// Tests that VideoCaptureDeviceFactory::CreateDeviceProxy() returns an error
// code when trying to create a device for an invalid descriptor.
TEST_F(VideoCaptureServiceTest,
       DISABLED_ErrorCodeOnCreateDeviceForInvalidDescriptor) {
  const std::string invalid_device_id = "invalid";
  base::RunLoop wait_loop;
  mojom::DevicePtr fake_device_proxy;
  base::MockCallback<mojom::DeviceFactory::CreateDeviceCallback>
      create_device_proxy_callback;
  EXPECT_CALL(create_device_proxy_callback,
              Run(mojom::DeviceAccessResultCode::ERROR_DEVICE_NOT_FOUND))
      .Times(1)
      .WillOnce(InvokeWithoutArgs([&wait_loop]() { wait_loop.Quit(); }));
  factory_->CreateDevice(invalid_device_id,
                         mojo::MakeRequest(&fake_device_proxy),
                         create_device_proxy_callback.Get());
  wait_loop.Run();
}

}  // namespace video_capture

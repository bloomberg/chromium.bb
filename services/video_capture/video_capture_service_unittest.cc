// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "services/video_capture/public/interfaces/video_capture_device_factory.mojom.h"
#include "services/video_capture/video_capture_service_test.h"

using testing::Exactly;
using testing::_;
using testing::Invoke;
using testing::InvokeWithoutArgs;

namespace video_capture {

class MockCreateDeviceProxyCallback {
 public:
  MOCK_METHOD1(Run, void(mojom::DeviceAccessResultCode result_code));
};

// Tests that an answer arrives from the service when calling
// EnumerateDeviceDescriptors().
TEST_F(VideoCaptureServiceTest, EnumerateDeviceDescriptorsCallbackArrives) {
  base::RunLoop wait_loop;
  EXPECT_CALL(descriptor_receiver_, OnEnumerateDeviceDescriptorsCallback(_))
      .Times(Exactly(1))
      .WillOnce(InvokeWithoutArgs([&wait_loop]() { wait_loop.Quit(); }));

  factory_->EnumerateDeviceDescriptors(base::Bind(
      &MockDeviceDescriptorReceiver::HandleEnumerateDeviceDescriptorsCallback,
      base::Unretained(&descriptor_receiver_)));
  wait_loop.Run();
}

TEST_F(VideoCaptureServiceTest, FakeDeviceFactoryEnumeratesOneDevice) {
  base::RunLoop wait_loop;
  size_t num_devices_enumerated = 0;
  EXPECT_CALL(descriptor_receiver_, OnEnumerateDeviceDescriptorsCallback(_))
      .Times(Exactly(1))
      .WillOnce(Invoke([&wait_loop, &num_devices_enumerated](
          const std::vector<mojom::VideoCaptureDeviceDescriptorPtr>&
              descriptors) {
        num_devices_enumerated = descriptors.size();
        wait_loop.Quit();
      }));

  factory_->EnumerateDeviceDescriptors(base::Bind(
      &MockDeviceDescriptorReceiver::HandleEnumerateDeviceDescriptorsCallback,
      base::Unretained(&descriptor_receiver_)));
  wait_loop.Run();
  ASSERT_EQ(1u, num_devices_enumerated);
}

// Tests that VideoCaptureDeviceFactory::CreateDeviceProxy() returns an error
// code when trying to create a device for an invalid descriptor.
TEST_F(VideoCaptureServiceTest, ErrorCodeOnCreateDeviceForInvalidDescriptor) {
  auto invalid_descriptor = mojom::VideoCaptureDeviceDescriptor::New();
  invalid_descriptor->device_id = "invalid";
  invalid_descriptor->model_id = "invalid";
  base::RunLoop wait_loop;
  mojom::VideoCaptureDeviceProxyPtr fake_device_proxy;
  MockCreateDeviceProxyCallback create_device_proxy_callback;
  EXPECT_CALL(create_device_proxy_callback,
              Run(mojom::DeviceAccessResultCode::ERROR_DEVICE_NOT_FOUND))
      .Times(1)
      .WillOnce(InvokeWithoutArgs([&wait_loop]() { wait_loop.Quit(); }));
  factory_->CreateDeviceProxy(
      std::move(invalid_descriptor), mojo::GetProxy(&fake_device_proxy),
      base::Bind(&MockCreateDeviceProxyCallback::Run,
                 base::Unretained(&create_device_proxy_callback)));
  wait_loop.Run();
}

}  // namespace video_capture

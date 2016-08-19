// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "services/shell/public/cpp/service_test.h"
#include "services/video_capture/public/interfaces/video_capture_device_factory.mojom.h"
#include "services/video_capture/public/interfaces/video_capture_service.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::Exactly;
using testing::_;
using testing::Invoke;
using testing::InvokeWithoutArgs;

namespace video_capture {

class MockDeviceDescriptorReceiver {
 public:
  // Use forwarding method to work around gmock not supporting move-only types.
  void HandleEnumerateDeviceDescriptorsCallback(
      std::vector<mojom::VideoCaptureDeviceDescriptorPtr> descriptors) {
    OnEnumerateDeviceDescriptorsCallback(descriptors);
  }

  MOCK_METHOD1(
      OnEnumerateDeviceDescriptorsCallback,
      void(const std::vector<mojom::VideoCaptureDeviceDescriptorPtr>&));
};

class VideoCaptureServiceTest : public shell::test::ServiceTest {
 public:
  VideoCaptureServiceTest()
      : shell::test::ServiceTest("exe:video_capture_unittests") {}
  ~VideoCaptureServiceTest() override {}

  void SetUp() override {
    ServiceTest::SetUp();
    connector()->ConnectToInterface("mojo:video_capture", &service_);
    service_->ConnectToFakeDeviceFactory(mojo::GetProxy(&factory_));
  }

 protected:
  mojom::VideoCaptureServicePtr service_;
  mojom::VideoCaptureDeviceFactoryPtr factory_;
  MockDeviceDescriptorReceiver descriptor_receiver_;
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

}  // namespace video_capture

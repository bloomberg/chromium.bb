// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "services/shell/public/cpp/service_test.h"
#include "services/video_capture/public/interfaces/video_capture_device_factory.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::Exactly;
using testing::_;

namespace video_capture {

ACTION_P(RunClosure, closure) {
  closure.Run();
}

class MockClient {
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
    connector()->ConnectToInterface("mojo:video_capture", &factory_);
  }

 protected:
  mojom::VideoCaptureDeviceFactoryPtr factory_;
  MockClient client_;
};

// Tests that an answer arrives from the service when calling
// EnumerateDeviceDescriptors().
TEST_F(VideoCaptureServiceTest, EnumerateDeviceDescriptorsCallbackArrives) {
  base::RunLoop wait_loop;
  EXPECT_CALL(client_, OnEnumerateDeviceDescriptorsCallback(_))
      .Times(Exactly(1))
      .WillOnce(RunClosure(wait_loop.QuitClosure()));

  factory_->EnumerateDeviceDescriptors(
      base::Bind(&MockClient::HandleEnumerateDeviceDescriptorsCallback,
                 base::Unretained(&client_)));
  wait_loop.Run();
}

}  // namespace video_capture

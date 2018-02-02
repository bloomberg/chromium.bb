// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/device_media_to_mojo_adapter.h"

#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "services/video_capture/test/mock_device.h"
#include "services/video_capture/test/mock_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Invoke;
using testing::_;

namespace video_capture {

class DeviceMediaToMojoAdapterTest : public ::testing::Test {
 public:
  DeviceMediaToMojoAdapterTest() = default;
  ~DeviceMediaToMojoAdapterTest() override = default;

  void SetUp() override {
    mock_receiver_ =
        std::make_unique<MockReceiver>(mojo::MakeRequest(&receiver_));
    auto mock_device = std::make_unique<MockDevice>();
    mock_device_ptr_ = mock_device.get();
    adapter_ = std::make_unique<DeviceMediaToMojoAdapter>(
        std::unique_ptr<service_manager::ServiceContextRef>(),
        std::move(mock_device), media::VideoCaptureJpegDecoderFactoryCB());
  }

  void TearDown() override {
    // The internals of ReceiverOnTaskRunner perform a DeleteSoon().
    adapter_.reset();
    base::RunLoop wait_loop;
    wait_loop.RunUntilIdle();
  }

 protected:
  MockDevice* mock_device_ptr_;
  std::unique_ptr<DeviceMediaToMojoAdapter> adapter_;
  std::unique_ptr<MockReceiver> mock_receiver_;
  mojom::ReceiverPtr receiver_;
  base::test::ScopedTaskEnvironment task_environment_;
};

TEST_F(DeviceMediaToMojoAdapterTest,
       DeviceIsStoppedWhenReceiverClosesConnection) {
  {
    base::RunLoop run_loop;
    EXPECT_CALL(*mock_device_ptr_, DoAllocateAndStart(_, _))
        .WillOnce(Invoke(
            [](const media::VideoCaptureParams& params,
               std::unique_ptr<media::VideoCaptureDevice::Client>* client) {
              (*client)->OnStarted();
            }));
    EXPECT_CALL(*mock_receiver_, OnStarted()).WillOnce(Invoke([&run_loop]() {
      run_loop.Quit();
    }));

    const media::VideoCaptureParams kArbitrarySettings;
    adapter_->Start(kArbitrarySettings, std::move(receiver_));
    run_loop.Run();
  }
  {
    base::RunLoop run_loop;
    EXPECT_CALL(*mock_device_ptr_, DoStopAndDeAllocate())
        .WillOnce(Invoke([&run_loop]() { run_loop.Quit(); }));
    mock_receiver_.reset();
    run_loop.Run();
  }
}

// Triggers a condition that caused a use-after-free reported in
// https://crbug.com/807887. The use-after-free happened because the connection
// lost event handler got invoked on a base::Unretained() pointer to |adapter_|
// after |adapter_| was released.
TEST_F(DeviceMediaToMojoAdapterTest,
       ReleaseInstanceSynchronouslyAfterReceiverClosedConnection) {
  {
    base::RunLoop run_loop;
    EXPECT_CALL(*mock_device_ptr_, DoAllocateAndStart(_, _))
        .WillOnce(Invoke(
            [](const media::VideoCaptureParams& params,
               std::unique_ptr<media::VideoCaptureDevice::Client>* client) {
              (*client)->OnStarted();
            }));
    EXPECT_CALL(*mock_receiver_, OnStarted()).WillOnce(Invoke([&run_loop]() {
      run_loop.Quit();
    }));

    const media::VideoCaptureParams kArbitrarySettings;
    adapter_->Start(kArbitrarySettings, std::move(receiver_));
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;

    // This posts invocation of the error event handler to the end of the
    // current sequence.
    mock_receiver_.reset();

    // This destroys the DeviceMediaToMojoAdapter, which in turn posts a
    // DeleteSoon in ~ReceiverOnTaskRunner() to the end of the current sequence.
    adapter_.reset();

    // Give error handle chance to get invoked
    run_loop.RunUntilIdle();
  }
}

}  // namespace video_capture

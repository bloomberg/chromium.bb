// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_task_environment.h"
#include "media/capture/video/mock_device.h"
#include "media/capture/video/mock_device_factory.h"
#include "media/capture/video/video_capture_device.h"
#include "media/capture/video/video_capture_system_impl.h"
#include "services/service_manager/public/cpp/service_keepalive.h"
#include "services/video_capture/device_factory_media_to_mojo_adapter.h"
#include "services/video_capture/public/cpp/mock_receiver.h"
#include "services/video_capture/public/mojom/device_factory_provider.mojom.h"
#include "services/video_capture/video_source_provider_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::InvokeWithoutArgs;
using testing::Mock;

namespace video_capture {

class MockDeviceSharedAccessTest : public ::testing::Test {
 public:
  MockDeviceSharedAccessTest() : service_keepalive_(nullptr, base::nullopt) {}
  ~MockDeviceSharedAccessTest() override {}

  void SetUp() override {
    auto mock_device_factory = std::make_unique<media::MockDeviceFactory>();
    media::VideoCaptureDeviceDescriptor mock_descriptor;
    mock_descriptor.device_id = "MockDeviceId";
    mock_device_factory->AddMockDevice(&mock_device_, mock_descriptor);

    auto video_capture_system = std::make_unique<media::VideoCaptureSystemImpl>(
        std::move(mock_device_factory));
    mock_device_factory_ = std::make_unique<DeviceFactoryMediaToMojoAdapter>(
        std::move(video_capture_system), base::DoNothing(),
        base::ThreadTaskRunnerHandle::Get());
    source_provider_ =
        std::make_unique<VideoSourceProviderImpl>(mock_device_factory_.get());
    source_provider_->SetServiceRef(service_keepalive_.CreateRef());

    // Obtain the mock device backed source from |sourcr_provider_|.
    base::MockCallback<mojom::DeviceFactory::GetDeviceInfosCallback>
        device_infos_receiver;
    base::RunLoop wait_loop;
    EXPECT_CALL(device_infos_receiver, Run(_))
        .WillOnce(InvokeWithoutArgs([&wait_loop]() { wait_loop.Quit(); }));
    source_provider_->GetSourceInfos(device_infos_receiver.Get());
    // We must wait for the response to GetDeviceInfos before calling
    // CreateDevice.
    wait_loop.Run();
    source_provider_->GetVideoSource(mock_descriptor.device_id,
                                     mojo::MakeRequest(&source_));
  }

 protected:
  base::test::ScopedTaskEnvironment task_environment_;
  media::MockDevice mock_device_;
  std::unique_ptr<DeviceFactoryMediaToMojoAdapter> mock_device_factory_;
  std::unique_ptr<VideoSourceProviderImpl> source_provider_;
  mojom::VideoSourcePtr source_;

 private:
  service_manager::ServiceKeepalive service_keepalive_;
};

// This alias ensures test output is easily attributed to this service's tests.
// TODO(chfremer): Consider just renaming the type.
using MockVideoCaptureDeviceSharedAccessTest = MockDeviceSharedAccessTest;

TEST_F(MockVideoCaptureDeviceSharedAccessTest, SourceIsBound) {
  ASSERT_TRUE(source_.is_bound());
}

}  // namespace video_capture

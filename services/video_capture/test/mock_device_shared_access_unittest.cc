// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_task_environment.h"
#include "media/capture/mojom/image_capture_types.h"
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
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::Mock;

namespace video_capture {

class MockDeviceSharedAccessTest : public ::testing::Test {
 public:
  MockDeviceSharedAccessTest()
      : mock_receiver_1_(mojo::MakeRequest(&receiver_1_)),
        mock_receiver_2_(mojo::MakeRequest(&receiver_2_)),
        service_keepalive_(nullptr, base::nullopt),
        next_arbitrary_frame_feedback_id_(123) {}
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

    requestable_settings_.requested_format.frame_size = gfx::Size(800, 600);
    requestable_settings_.requested_format.frame_rate = 15;
    requestable_settings_.requested_format.pixel_format =
        media::PIXEL_FORMAT_I420;
    requestable_settings_.resolution_change_policy =
        media::ResolutionChangePolicy::FIXED_RESOLUTION;
    requestable_settings_.power_line_frequency =
        media::PowerLineFrequency::FREQUENCY_DEFAULT;
  }

  void LetClient1ConnectWithRequestableSettingsAndExpectToGetThem() {
    base::RunLoop run_loop;
    source_->CreatePushSubscription(
        std::move(receiver_1_), requestable_settings_,
        false /*force_reopen_with_new_settings*/,
        mojo::MakeRequest(&subscription_1_),
        base::BindOnce(
            [](base::RunLoop* run_loop,
               media::VideoCaptureParams* requested_settings,
               mojom::CreatePushSubscriptionResultCode result_code,
               const media::VideoCaptureParams&
                   settings_source_was_opened_with) {
              ASSERT_EQ(mojom::CreatePushSubscriptionResultCode::
                            kCreatedWithRequestedSettings,
                        result_code);
              ASSERT_EQ(*requested_settings, settings_source_was_opened_with);
              run_loop->Quit();
            },
            &run_loop, &requestable_settings_));
    run_loop.Run();
  }

  void LetClient2ConnectWithRequestableSettingsAndExpectToGetThem() {
    base::RunLoop run_loop;
    source_->CreatePushSubscription(
        std::move(receiver_2_), requestable_settings_,
        false /*force_reopen_with_new_settings*/,
        mojo::MakeRequest(&subscription_2_),
        base::BindOnce(
            [](base::RunLoop* run_loop,
               media::VideoCaptureParams* requested_settings,
               mojom::CreatePushSubscriptionResultCode result_code,
               const media::VideoCaptureParams&
                   settings_source_was_opened_with) {
              ASSERT_EQ(mojom::CreatePushSubscriptionResultCode::
                            kCreatedWithRequestedSettings,
                        result_code);
              ASSERT_EQ(*requested_settings, settings_source_was_opened_with);
              run_loop->Quit();
            },
            &run_loop, &requestable_settings_));
    run_loop.Run();
  }

  void LetTwoClientsConnectWithSameSettingsOneByOne() {
    LetClient1ConnectWithRequestableSettingsAndExpectToGetThem();
    LetClient2ConnectWithRequestableSettingsAndExpectToGetThem();
  }

  void LetTwoClientsConnectWithDifferentSettings() {
    base::RunLoop run_loop_1;
    base::RunLoop run_loop_2;
    source_->CreatePushSubscription(
        std::move(receiver_1_), requestable_settings_,
        false /*force_reopen_with_new_settings*/,
        mojo::MakeRequest(&subscription_1_),
        base::BindOnce(
            [](base::RunLoop* run_loop_1,
               media::VideoCaptureParams* requested_settings,
               mojom::CreatePushSubscriptionResultCode result_code,
               const media::VideoCaptureParams&
                   settings_source_was_opened_with) {
              ASSERT_EQ(mojom::CreatePushSubscriptionResultCode::
                            kCreatedWithRequestedSettings,
                        result_code);
              ASSERT_EQ(*requested_settings, settings_source_was_opened_with);
              run_loop_1->Quit();
            },
            &run_loop_1, &requestable_settings_));

    auto different_settings = requestable_settings_;
    // Change something arbitrary
    different_settings.requested_format.frame_size = gfx::Size(123, 456);
    ASSERT_FALSE(requestable_settings_ == different_settings);

    source_->CreatePushSubscription(
        std::move(receiver_2_), different_settings,
        false /*force_reopen_with_new_settings*/,
        mojo::MakeRequest(&subscription_2_),
        base::BindOnce(
            [](base::RunLoop* run_loop_2,
               media::VideoCaptureParams* requested_settings,
               mojom::CreatePushSubscriptionResultCode result_code,
               const media::VideoCaptureParams&
                   settings_source_was_opened_with) {
              ASSERT_EQ(mojom::CreatePushSubscriptionResultCode::
                            kCreatedWithDifferentSettings,
                        result_code);
              ASSERT_EQ(*requested_settings, settings_source_was_opened_with);
              run_loop_2->Quit();
            },
            &run_loop_2, &requestable_settings_));

    run_loop_1.Run();
    run_loop_2.Run();
  }

  void SendFrameAndExpectToArriveAtBothSubscribers() {
    const int32_t kArbitraryFrameFeedbackId =
        next_arbitrary_frame_feedback_id_++;
    const int32_t kArbitraryRotation = 0;
    base::RunLoop wait_loop_1;
    EXPECT_CALL(mock_receiver_1_,
                DoOnFrameReadyInBuffer(_, kArbitraryFrameFeedbackId, _, _))
        .WillOnce(InvokeWithoutArgs([&wait_loop_1]() { wait_loop_1.Quit(); }));
    base::RunLoop wait_loop_2;
    EXPECT_CALL(mock_receiver_2_,
                DoOnFrameReadyInBuffer(_, kArbitraryFrameFeedbackId, _, _))
        .WillOnce(InvokeWithoutArgs([&wait_loop_2]() { wait_loop_2.Quit(); }));
    mock_device_.SendStubFrame(requestable_settings_.requested_format,
                               kArbitraryRotation, kArbitraryFrameFeedbackId);
    wait_loop_1.Run();
    wait_loop_2.Run();
    Mock::VerifyAndClearExpectations(&mock_receiver_1_);
    Mock::VerifyAndClearExpectations(&mock_receiver_2_);
  }

  void SendFrameAndExpectToArriveOnlyAtSubscriber2() {
    const int32_t kArbitraryFrameFeedbackId =
        next_arbitrary_frame_feedback_id_++;
    const int32_t kArbitraryRotation = 0;

    base::RunLoop wait_loop;
    EXPECT_CALL(mock_receiver_1_, DoOnFrameReadyInBuffer(_, _, _, _)).Times(0);
    EXPECT_CALL(mock_receiver_2_,
                DoOnFrameReadyInBuffer(_, kArbitraryFrameFeedbackId, _, _))
        .WillOnce(InvokeWithoutArgs([&wait_loop]() { wait_loop.Quit(); }));
    mock_device_.SendStubFrame(requestable_settings_.requested_format,
                               kArbitraryRotation, kArbitraryFrameFeedbackId);
    wait_loop.Run();
    Mock::VerifyAndClearExpectations(&mock_receiver_1_);
    Mock::VerifyAndClearExpectations(&mock_receiver_2_);
  }

 protected:
  base::test::ScopedTaskEnvironment task_environment_;
  media::MockDevice mock_device_;
  std::unique_ptr<DeviceFactoryMediaToMojoAdapter> mock_device_factory_;
  std::unique_ptr<VideoSourceProviderImpl> source_provider_;
  mojom::VideoSourcePtr source_;
  media::VideoCaptureParams requestable_settings_;

  mojom::PushVideoStreamSubscriptionPtr subscription_1_;
  mojom::ReceiverPtr receiver_1_;
  MockReceiver mock_receiver_1_;
  mojom::PushVideoStreamSubscriptionPtr subscription_2_;
  mojom::ReceiverPtr receiver_2_;
  MockReceiver mock_receiver_2_;

 private:
  service_manager::ServiceKeepalive service_keepalive_;
  int32_t next_arbitrary_frame_feedback_id_;
};

// This alias ensures test output is easily attributed to this service's tests.
// TODO(chfremer): Consider just renaming the type.
using MockVideoCaptureDeviceSharedAccessTest = MockDeviceSharedAccessTest;

TEST_F(MockVideoCaptureDeviceSharedAccessTest,
       TwoClientsCreatePushSubscriptionWithSameSettings) {
  LetTwoClientsConnectWithSameSettingsOneByOne();
}

TEST_F(MockVideoCaptureDeviceSharedAccessTest,
       TwoClientsCreatePushSubscriptionWithDifferentSettings) {
  LetTwoClientsConnectWithDifferentSettings();
}

TEST_F(MockVideoCaptureDeviceSharedAccessTest,
       NoFramesArePushedUntilSubscriptionIsActivated) {
  LetTwoClientsConnectWithSameSettingsOneByOne();

  subscription_2_->Activate();
  SendFrameAndExpectToArriveOnlyAtSubscriber2();

  subscription_1_->Activate();
  SendFrameAndExpectToArriveAtBothSubscribers();
}

TEST_F(MockVideoCaptureDeviceSharedAccessTest,
       DiscardingLastSubscriptionStopsTheDevice) {
  EXPECT_CALL(mock_device_, DoAllocateAndStart(_, _)).Times(1);
  EXPECT_CALL(mock_device_, DoStopAndDeAllocate()).Times(0);
  LetTwoClientsConnectWithDifferentSettings();
  subscription_1_.reset();
  Mock::VerifyAndClearExpectations(&mock_device_);

  base::RunLoop wait_loop;
  EXPECT_CALL(mock_device_, DoStopAndDeAllocate())
      .WillOnce(InvokeWithoutArgs([&wait_loop]() { wait_loop.Quit(); }));
  subscription_2_.reset();
  wait_loop.Run();
}

TEST_F(MockVideoCaptureDeviceSharedAccessTest,
       DiscardingVideoSourceWithActiveSubscriptionsStopsTheDevice) {
  LetTwoClientsConnectWithDifferentSettings();
  subscription_1_->Activate();
  subscription_2_->Activate();

  base::RunLoop wait_loop;
  EXPECT_CALL(mock_device_, DoStopAndDeAllocate())
      .WillOnce(InvokeWithoutArgs([&wait_loop]() { wait_loop.Quit(); }));
  source_.reset();
  wait_loop.Run();
}

TEST_F(MockVideoCaptureDeviceSharedAccessTest,
       NoMoreFramesArriveAfterClosingSubscription) {
  LetTwoClientsConnectWithDifferentSettings();
  subscription_1_->Activate();
  subscription_2_->Activate();

  SendFrameAndExpectToArriveAtBothSubscribers();

  {
    base::RunLoop wait_loop;
    subscription_1_->Close(base::BindOnce(
        [](base::RunLoop* wait_loop) { wait_loop->Quit(); }, &wait_loop));
    wait_loop.Run();
  }

  SendFrameAndExpectToArriveOnlyAtSubscriber2();
}

TEST_F(MockVideoCaptureDeviceSharedAccessTest, SuspendAndResume) {
  LetTwoClientsConnectWithDifferentSettings();
  subscription_1_->Activate();
  subscription_2_->Activate();

  SendFrameAndExpectToArriveAtBothSubscribers();

  {
    base::RunLoop wait_loop;
    subscription_1_->Suspend(base::BindOnce(
        [](base::RunLoop* wait_loop) { wait_loop->Quit(); }, &wait_loop));
    wait_loop.Run();
  }

  SendFrameAndExpectToArriveOnlyAtSubscriber2();

  subscription_1_->Resume();
  SendFrameAndExpectToArriveAtBothSubscribers();
}

TEST_F(MockVideoCaptureDeviceSharedAccessTest,
       CreateNewSubscriptionAfterClosingExistingOneUsesNewSettings) {
  LetClient1ConnectWithRequestableSettingsAndExpectToGetThem();
  base::RunLoop run_loop;
  subscription_1_->Close(base::BindOnce(
      [](base::RunLoop* run_loop) { run_loop->Quit(); }, &run_loop));
  run_loop.Run();

  // modify settings to check if they will be applied
  const int kArbitraryDifferentWidth = 753;
  requestable_settings_.requested_format.frame_size.set_width(
      kArbitraryDifferentWidth);
  LetClient2ConnectWithRequestableSettingsAndExpectToGetThem();
}

TEST_F(MockVideoCaptureDeviceSharedAccessTest,
       TakePhotoUsingOnPushSubscription) {
  LetTwoClientsConnectWithDifferentSettings();

  {
    EXPECT_CALL(mock_device_, DoGetPhotoState(_))
        .WillOnce(Invoke(
            [](media::VideoCaptureDevice::GetPhotoStateCallback* callback) {
              media::mojom::PhotoStatePtr state = mojo::CreateEmptyPhotoState();
              std::move(*callback).Run(std::move(state));
            }));
    base::RunLoop run_loop;
    subscription_1_->GetPhotoState(base::BindOnce(
        [](base::RunLoop* run_loop, media::mojom::PhotoStatePtr state) {
          run_loop->Quit();
        },
        &run_loop));

    run_loop.Run();
  }

  {
    EXPECT_CALL(mock_device_, DoSetPhotoOptions(_, _))
        .WillOnce(Invoke(
            [](media::mojom::PhotoSettingsPtr* settings,
               media::VideoCaptureDevice::SetPhotoOptionsCallback* callback) {
              media::mojom::BlobPtr blob = media::mojom::Blob::New();
              std::move(*callback).Run(true);
            }));
    media::mojom::PhotoSettingsPtr settings =
        media::mojom::PhotoSettings::New();
    base::RunLoop run_loop;
    subscription_1_->SetPhotoOptions(
        std::move(settings), base::BindOnce(
                                 [](base::RunLoop* run_loop, bool succeeded) {
                                   ASSERT_TRUE(succeeded);
                                   run_loop->Quit();
                                 },
                                 &run_loop));
    run_loop.Run();
  }

  {
    EXPECT_CALL(mock_device_, DoTakePhoto(_))
        .WillOnce(
            Invoke([](media::VideoCaptureDevice::TakePhotoCallback* callback) {
              media::mojom::BlobPtr blob = media::mojom::Blob::New();
              std::move(*callback).Run(std::move(blob));
            }));
    base::RunLoop run_loop;
    subscription_1_->TakePhoto(
        base::BindOnce([](base::RunLoop* run_loop,
                          media::mojom::BlobPtr state) { run_loop->Quit(); },
                       &run_loop));

    run_loop.Run();
  }
}

}  // namespace video_capture

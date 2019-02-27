// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/media/video_wake_lock.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/picture_in_picture/picture_in_picture.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/frame/picture_in_picture_controller.h"
#include "third_party/blink/renderer/core/html/media/html_media_test_helper.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/testing/wait_for_event.h"
#include "third_party/blink/renderer/platform/testing/empty_web_media_player.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

// The VideoWakeLockPictureInPictureService implements the PictureInPicture
// service in the same process as the test and guarantees that the callbacks are
// called in order for the events to be fired. set_run_loop() MUST be called
// before each attempt to enter/leave Picture-in-Picture.
class VideoWakeLockPictureInPictureService
    : public mojom::blink::PictureInPictureService {
 public:
  VideoWakeLockPictureInPictureService() : binding_(this) {}
  ~VideoWakeLockPictureInPictureService() override = default;

  void Bind(mojo::ScopedMessagePipeHandle handle) {
    binding_.Bind(
        mojom::blink::PictureInPictureServiceRequest(std::move(handle)));
  }

  void StartSession(uint32_t,
                    const base::Optional<viz::SurfaceId>&,
                    const blink::WebSize&,
                    bool,
                    bool,
                    StartSessionCallback callback) final {
    std::move(callback).Run(WebSize());
  }

  void EndSession(EndSessionCallback callback) final {
    std::move(callback).Run();
  }

  void UpdateSession(uint32_t,
                     const base::Optional<viz::SurfaceId>&,
                     const blink::WebSize&,
                     bool,
                     bool) final {}
  void SetDelegate(mojom::blink::PictureInPictureDelegatePtr) final {}

 private:
  mojo::Binding<mojom::blink::PictureInPictureService> binding_;
};

class VideoWakeLockMediaPlayer final : public EmptyWebMediaPlayer {
 public:
  ReadyState GetReadyState() const final { return kReadyStateHaveMetadata; }
  bool HasVideo() const final { return true; }
};

class VideoWakeLockFrameClient : public test::MediaStubLocalFrameClient {
 public:
  static VideoWakeLockFrameClient* Create(
      std::unique_ptr<WebMediaPlayer> player) {
    return MakeGarbageCollected<VideoWakeLockFrameClient>(std::move(player));
  }

  explicit VideoWakeLockFrameClient(std::unique_ptr<WebMediaPlayer> player)
      : test::MediaStubLocalFrameClient(std::move(player)),
        interface_provider_(new service_manager::InterfaceProvider()) {}

  service_manager::InterfaceProvider* GetInterfaceProvider() override {
    return interface_provider_.get();
  }

 private:
  std::unique_ptr<service_manager::InterfaceProvider> interface_provider_;

  DISALLOW_COPY_AND_ASSIGN(VideoWakeLockFrameClient);
};

class VideoWakeLockTest : public PageTestBase {
 public:
  void SetUp() override {
    PageTestBase::SetupPageWithClients(
        nullptr, VideoWakeLockFrameClient::Create(
                     std::make_unique<VideoWakeLockMediaPlayer>()));

    service_manager::InterfaceProvider::TestApi test_api(
        GetFrame().Client()->GetInterfaceProvider());
    test_api.SetBinderForName(
        mojom::blink::PictureInPictureService::Name_,
        WTF::BindRepeating(&VideoWakeLockPictureInPictureService::Bind,
                           WTF::Unretained(&pip_service_)));

    video_ = HTMLVideoElement::Create(GetDocument());
    video_->SetReadyState(HTMLMediaElement::ReadyState::kHaveMetadata);
    video_wake_lock_ = MakeGarbageCollected<VideoWakeLock>(*video_.Get());

    GetPage().SetIsHidden(false, true);
  }

  HTMLVideoElement* Video() const { return video_.Get(); }
  VideoWakeLock* GetVideoWakeLock() const { return video_wake_lock_.Get(); }

  void SetFakeCcLayer(cc::Layer* layer) { video_->SetCcLayer(layer); }

  void SimulatePlaying() {
    video_wake_lock_->Invoke(&GetDocument(),
                             Event::Create(event_type_names::kPlaying));
  }

  void SimulatePause() {
    video_wake_lock_->Invoke(&GetDocument(),
                             Event::Create(event_type_names::kPause));
  }

  void SimulateEnterPictureInPicture() {
    PictureInPictureController::From(GetDocument())
        .EnterPictureInPicture(Video(), nullptr);

    WaitForEvent::Create(video_.Get(),
                         event_type_names::kEnterpictureinpicture);
  }

  void SimulateLeavePictureInPicture() {
    PictureInPictureController::From(GetDocument())
        .ExitPictureInPicture(Video(), nullptr);

    WaitForEvent::Create(video_.Get(),
                         event_type_names::kLeavepictureinpicture);
  }

 private:
  Persistent<HTMLVideoElement> video_;
  Persistent<VideoWakeLock> video_wake_lock_;

  VideoWakeLockPictureInPictureService pip_service_;
};

TEST_F(VideoWakeLockTest, NoLockByDefault) {
  EXPECT_FALSE(GetVideoWakeLock()->active_for_tests());
}

TEST_F(VideoWakeLockTest, PlayingVideoRequestsLock) {
  SimulatePlaying();
  EXPECT_TRUE(GetVideoWakeLock()->active_for_tests());
}

TEST_F(VideoWakeLockTest, PausingVideoCancelsLock) {
  SimulatePlaying();
  EXPECT_TRUE(GetVideoWakeLock()->active_for_tests());

  SimulatePause();
  EXPECT_FALSE(GetVideoWakeLock()->active_for_tests());
}

TEST_F(VideoWakeLockTest, HiddingPageCancelsLock) {
  SimulatePlaying();
  EXPECT_TRUE(GetVideoWakeLock()->active_for_tests());

  GetPage().SetIsHidden(true, false);
  EXPECT_FALSE(GetVideoWakeLock()->active_for_tests());
}

TEST_F(VideoWakeLockTest, PlayingWhileHiddenDoesNotRequestLock) {
  GetPage().SetIsHidden(true, false);
  SimulatePlaying();
  EXPECT_FALSE(GetVideoWakeLock()->active_for_tests());
}

TEST_F(VideoWakeLockTest, ShowingPageRequestsLock) {
  SimulatePlaying();
  GetPage().SetIsHidden(true, false);
  EXPECT_FALSE(GetVideoWakeLock()->active_for_tests());

  GetPage().SetIsHidden(false, false);
  EXPECT_TRUE(GetVideoWakeLock()->active_for_tests());
}

TEST_F(VideoWakeLockTest, ShowingPageDoNotRequestsLockIfPaused) {
  SimulatePlaying();
  GetPage().SetIsHidden(true, false);
  EXPECT_FALSE(GetVideoWakeLock()->active_for_tests());

  SimulatePause();
  GetPage().SetIsHidden(false, false);
  EXPECT_FALSE(GetVideoWakeLock()->active_for_tests());
}

TEST_F(VideoWakeLockTest, RemotePlaybackDisconnectedDoesNotCancelLock) {
  SimulatePlaying();
  GetVideoWakeLock()->OnRemotePlaybackStateChanged(
      mojom::blink::PresentationConnectionState::CLOSED);
  EXPECT_TRUE(GetVideoWakeLock()->active_for_tests());
}

TEST_F(VideoWakeLockTest, RemotePlaybackConnectingDoesNotCancelLock) {
  SimulatePlaying();
  GetVideoWakeLock()->OnRemotePlaybackStateChanged(
      mojom::blink::PresentationConnectionState::CONNECTING);
  EXPECT_TRUE(GetVideoWakeLock()->active_for_tests());
}

TEST_F(VideoWakeLockTest, ActiveRemotePlaybackCancelsLock) {
  SimulatePlaying();
  GetVideoWakeLock()->OnRemotePlaybackStateChanged(
      mojom::blink::PresentationConnectionState::CLOSED);
  EXPECT_TRUE(GetVideoWakeLock()->active_for_tests());

  GetVideoWakeLock()->OnRemotePlaybackStateChanged(
      mojom::blink::PresentationConnectionState::CONNECTED);
  EXPECT_FALSE(GetVideoWakeLock()->active_for_tests());
}

TEST_F(VideoWakeLockTest, LeavingRemotePlaybackResumesLock) {
  SimulatePlaying();
  GetVideoWakeLock()->OnRemotePlaybackStateChanged(
      mojom::blink::PresentationConnectionState::CONNECTED);
  EXPECT_FALSE(GetVideoWakeLock()->active_for_tests());

  GetVideoWakeLock()->OnRemotePlaybackStateChanged(
      mojom::blink::PresentationConnectionState::CLOSED);
  EXPECT_TRUE(GetVideoWakeLock()->active_for_tests());
}

TEST_F(VideoWakeLockTest, PictureInPictureLocksWhenPageNotVisible) {
  // This initialeses the video element in order to not crash when the
  // interstitial tries to show itself and so that the WebMediaPlayer is set up.
  scoped_refptr<cc::Layer> layer = cc::Layer::Create();
  SetFakeCcLayer(layer.get());
  Video()->SetSrc("http://example.com/foo.mp4");
  test::RunPendingTasks();

  SimulatePlaying();
  GetPage().SetIsHidden(true, false);
  EXPECT_FALSE(GetVideoWakeLock()->active_for_tests());

  SimulateEnterPictureInPicture();
  EXPECT_TRUE(GetVideoWakeLock()->active_for_tests());
}

TEST_F(VideoWakeLockTest, PictureInPictureDoesNoLockWhenPaused) {
  // This initialeses the video element in order to not crash when the
  // interstitial tries to show itself and so that the WebMediaPlayer is set up.
  scoped_refptr<cc::Layer> layer = cc::Layer::Create();
  SetFakeCcLayer(layer.get());
  Video()->SetSrc("http://example.com/foo.mp4");
  test::RunPendingTasks();

  SimulatePlaying();
  GetPage().SetIsHidden(true, false);
  EXPECT_FALSE(GetVideoWakeLock()->active_for_tests());

  SimulatePause();
  SimulateEnterPictureInPicture();
  EXPECT_FALSE(GetVideoWakeLock()->active_for_tests());
}

TEST_F(VideoWakeLockTest, LeavingPictureInPictureCancelsLock) {
  // This initialeses the video element in order to not crash when the
  // interstitial tries to show itself and so that the WebMediaPlayer is set up.
  scoped_refptr<cc::Layer> layer = cc::Layer::Create();
  SetFakeCcLayer(layer.get());
  Video()->SetSrc("http://example.com/foo.mp4");
  test::RunPendingTasks();

  SimulatePlaying();
  GetPage().SetIsHidden(true, false);
  SimulateEnterPictureInPicture();
  EXPECT_TRUE(GetVideoWakeLock()->active_for_tests());

  SimulateLeavePictureInPicture();
  EXPECT_FALSE(GetVideoWakeLock()->active_for_tests());
}

TEST_F(VideoWakeLockTest, RemotingVideoInPictureInPictureDoesNotRequestLock) {
  // This initialeses the video element in order to not crash when the
  // interstitial tries to show itself and so that the WebMediaPlayer is set up.
  scoped_refptr<cc::Layer> layer = cc::Layer::Create();
  SetFakeCcLayer(layer.get());
  Video()->SetSrc("http://example.com/foo.mp4");
  test::RunPendingTasks();

  SimulatePlaying();
  SimulateEnterPictureInPicture();
  GetVideoWakeLock()->OnRemotePlaybackStateChanged(
      mojom::blink::PresentationConnectionState::CONNECTED);
  EXPECT_FALSE(GetVideoWakeLock()->active_for_tests());
}

}  // namespace blink

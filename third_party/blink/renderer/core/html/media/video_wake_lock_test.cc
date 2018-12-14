// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/media/video_wake_lock.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/frame/picture_in_picture_controller.h"
#include "third_party/blink/renderer/core/html/media/html_media_test_helper.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/empty_web_media_player.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

class VideoWakeLockMediaPlayer : public EmptyWebMediaPlayer {
 public:
  void EnterPictureInPicture(PipWindowOpenedCallback callback) final {
    std::move(callback).Run(WebSize());
  }

  void ExitPictureInPicture(PipWindowClosedCallback callback) final {
    std::move(callback).Run();
  }
};

class VideoWakeLockTest : public PageTestBase {
 public:
  void SetUp() override {
    PageTestBase::SetupPageWithClients(
        nullptr, test::MediaStubLocalFrameClient::Create(
                     std::make_unique<VideoWakeLockMediaPlayer>()));

    video_ = HTMLVideoElement::Create(GetDocument());
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
  }

  void SimulateLeavePictureInPicture() {
    PictureInPictureController::From(GetDocument())
        .ExitPictureInPicture(Video(), nullptr);
  }

 private:
  Persistent<HTMLVideoElement> video_;
  Persistent<VideoWakeLock> video_wake_lock_;
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
      WebRemotePlaybackState::kDisconnected);
  EXPECT_TRUE(GetVideoWakeLock()->active_for_tests());
}

TEST_F(VideoWakeLockTest, RemotePlaybackConnectingDoesNotCancelLock) {
  SimulatePlaying();
  GetVideoWakeLock()->OnRemotePlaybackStateChanged(
      WebRemotePlaybackState::kConnecting);
  EXPECT_TRUE(GetVideoWakeLock()->active_for_tests());
}

TEST_F(VideoWakeLockTest, ActiveRemotePlaybackCancelsLock) {
  SimulatePlaying();
  GetVideoWakeLock()->OnRemotePlaybackStateChanged(
      WebRemotePlaybackState::kDisconnected);
  EXPECT_TRUE(GetVideoWakeLock()->active_for_tests());

  GetVideoWakeLock()->OnRemotePlaybackStateChanged(
      WebRemotePlaybackState::kConnected);
  EXPECT_FALSE(GetVideoWakeLock()->active_for_tests());
}

TEST_F(VideoWakeLockTest, LeavingRemotePlaybackResumesLock) {
  SimulatePlaying();
  GetVideoWakeLock()->OnRemotePlaybackStateChanged(
      WebRemotePlaybackState::kConnected);
  EXPECT_FALSE(GetVideoWakeLock()->active_for_tests());

  GetVideoWakeLock()->OnRemotePlaybackStateChanged(
      WebRemotePlaybackState::kDisconnected);
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
      WebRemotePlaybackState::kConnected);
  EXPECT_FALSE(GetVideoWakeLock()->active_for_tests());
}

}  // namespace blink

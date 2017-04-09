// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/media_controls/MediaControlsOrientationLockDelegate.h"

#include "core/dom/Document.h"
#include "core/dom/DocumentUserGestureToken.h"
#include "core/dom/Fullscreen.h"
#include "core/frame/ScreenOrientationController.h"
#include "core/html/HTMLAudioElement.h"
#include "core/html/HTMLVideoElement.h"
#include "core/loader/EmptyClients.h"
#include "core/testing/DummyPageHolder.h"
#include "modules/media_controls/MediaControlsImpl.h"
#include "platform/UserGestureIndicator.h"
#include "platform/testing/EmptyWebMediaPlayer.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/WebSize.h"
#include "public/platform/modules/screen_orientation/WebLockOrientationCallback.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;

namespace blink {

namespace {

// WebLockOrientationCallback implementation that will not react to a success
// nor a failure.
class DummyScreenOrientationCallback : public WebLockOrientationCallback {
 public:
  void OnSuccess() override {}
  void OnError(WebLockOrientationError) override {}
};

class MockVideoWebMediaPlayer : public EmptyWebMediaPlayer {
 public:
  bool HasVideo() const override { return true; }

  MOCK_CONST_METHOD0(NaturalSize, WebSize());
};

class MockChromeClient : public EmptyChromeClient {
 public:
  MOCK_CONST_METHOD0(GetScreenInfo, WebScreenInfo());
};

class StubLocalFrameClient : public EmptyLocalFrameClient {
 public:
  static StubLocalFrameClient* Create() { return new StubLocalFrameClient; }

  std::unique_ptr<WebMediaPlayer> CreateWebMediaPlayer(
      HTMLMediaElement&,
      const WebMediaPlayerSource&,
      WebMediaPlayerClient*) override {
    return WTF::WrapUnique(new MockVideoWebMediaPlayer());
  }
};

class MockScreenOrientationController final
    : public ScreenOrientationController {
  WTF_MAKE_NONCOPYABLE(MockScreenOrientationController);

 public:
  static MockScreenOrientationController* ProvideTo(LocalFrame& frame) {
    MockScreenOrientationController* controller =
        new MockScreenOrientationController(frame);
    ScreenOrientationController::ProvideTo(frame, controller);
    return controller;
  }

  MOCK_METHOD1(lock, void(WebScreenOrientationLockType));
  MOCK_METHOD0(MockUnlock, void());

  DEFINE_INLINE_VIRTUAL_TRACE() { ScreenOrientationController::Trace(visitor); }

 private:
  explicit MockScreenOrientationController(LocalFrame& frame)
      : ScreenOrientationController(frame) {}

  void lock(WebScreenOrientationLockType type,
            std::unique_ptr<WebLockOrientationCallback>) override {
    locked_ = true;
    lock(type);
  }

  void unlock() override {
    locked_ = false;
    MockUnlock();
  }

  void NotifyOrientationChanged() override {}

  bool MaybeHasActiveLock() const override { return locked_; }

  bool locked_ = false;
};

}  // anonymous namespace

class MediaControlsOrientationLockDelegateTest : public ::testing::Test {
 protected:
  void SetUp() override {
    previous_video_fullscreen_orientation_lock_value_ =
        RuntimeEnabledFeatures::videoFullscreenOrientationLockEnabled();
    RuntimeEnabledFeatures::setVideoFullscreenOrientationLockEnabled(true);

    chrome_client_ = new MockChromeClient();

    Page::PageClients clients;
    FillWithEmptyClients(clients);
    clients.chrome_client = chrome_client_.Get();

    page_holder_ = DummyPageHolder::Create(IntSize(800, 600), &clients,
                                           StubLocalFrameClient::Create());

    GetDocument().write("<body><video></body>");
    video_ = toHTMLVideoElement(*GetDocument().QuerySelector("video"));

    screen_orientation_controller_ =
        MockScreenOrientationController::ProvideTo(page_holder_->GetFrame());
  }

  void TearDown() override {
    ::testing::Mock::VerifyAndClear(&GetScreenOrientationController());

    RuntimeEnabledFeatures::setVideoFullscreenOrientationLockEnabled(
        previous_video_fullscreen_orientation_lock_value_);
  }

  static bool HasDelegate(const MediaControls& media_controls) {
    return !!static_cast<const MediaControlsImpl*>(&media_controls)
                 ->orientation_lock_delegate_;
  }

  void SimulateEnterFullscreen() {
    UserGestureIndicator gesture(
        DocumentUserGestureToken::Create(&GetDocument()));

    Fullscreen::RequestFullscreen(Video());
    Fullscreen::From(GetDocument()).DidEnterFullscreen();
    testing::RunPendingTasks();
  }

  void SimulateExitFullscreen() {
    Fullscreen::ExitFullscreen(GetDocument());
    Fullscreen::From(GetDocument()).DidExitFullscreen();
    testing::RunPendingTasks();
  }

  void SimulateOrientationLock() {
    ScreenOrientationController* controller =
        ScreenOrientationController::From(*GetDocument().GetFrame());
    controller->lock(kWebScreenOrientationLockLandscape,
                     WTF::WrapUnique(new DummyScreenOrientationCallback));
    EXPECT_TRUE(controller->MaybeHasActiveLock());
  }

  void SimulateVideoReadyState(HTMLMediaElement::ReadyState state) {
    Video().SetReadyState(state);
  }

  void SimulateVideoNetworkState(HTMLMediaElement::NetworkState state) {
    Video().SetNetworkState(state);
  }

  MediaControlsImpl* MediaControls() const {
    return static_cast<MediaControlsImpl*>(video_->GetMediaControls());
  }

  void CheckStatePendingFullscreen() const {
    EXPECT_EQ(MediaControlsOrientationLockDelegate::State::kPendingFullscreen,
              MediaControls()->orientation_lock_delegate_->state_);
  }

  void CheckStatePendingMetadata() const {
    EXPECT_EQ(MediaControlsOrientationLockDelegate::State::kPendingMetadata,
              MediaControls()->orientation_lock_delegate_->state_);
  }

  void CheckStateMaybeLockedFullscreen() const {
    EXPECT_EQ(
        MediaControlsOrientationLockDelegate::State::kMaybeLockedFullscreen,
        MediaControls()->orientation_lock_delegate_->state_);
  }

  bool DelegateWillUnlockFullscreen() const {
    return MediaControls()
        ->orientation_lock_delegate_->should_unlock_orientation_;
  }

  WebScreenOrientationLockType ComputeOrientationLock() const {
    return MediaControls()
        ->orientation_lock_delegate_->ComputeOrientationLock();
  }

  MockChromeClient& ChromeClient() const { return *chrome_client_; }

  HTMLVideoElement& Video() const { return *video_; }
  Document& GetDocument() const { return page_holder_->GetDocument(); }
  MockScreenOrientationController& GetScreenOrientationController() const {
    return *screen_orientation_controller_;
  }
  MockVideoWebMediaPlayer& MockWebMediaPlayer() const {
    return *static_cast<MockVideoWebMediaPlayer*>(Video().GetWebMediaPlayer());
  }

 private:
  bool previous_video_fullscreen_orientation_lock_value_;
  std::unique_ptr<DummyPageHolder> page_holder_;
  Persistent<HTMLVideoElement> video_;
  Persistent<MockScreenOrientationController> screen_orientation_controller_;
  Persistent<MockChromeClient> chrome_client_;
};

TEST_F(MediaControlsOrientationLockDelegateTest, DelegateRequiresFlag) {
  // Flag on by default.
  EXPECT_TRUE(HasDelegate(*Video().GetMediaControls()));

  // Same with flag off.
  RuntimeEnabledFeatures::setVideoFullscreenOrientationLockEnabled(false);
  HTMLVideoElement* video = HTMLVideoElement::Create(GetDocument());
  GetDocument().body()->AppendChild(video);
  EXPECT_FALSE(HasDelegate(*video->GetMediaControls()));
}

TEST_F(MediaControlsOrientationLockDelegateTest, DelegateRequiresVideo) {
  HTMLAudioElement* audio = HTMLAudioElement::Create(GetDocument());
  GetDocument().body()->AppendChild(audio);
  EXPECT_FALSE(HasDelegate(*audio->GetMediaControls()));
}

TEST_F(MediaControlsOrientationLockDelegateTest, InitialState) {
  CheckStatePendingFullscreen();
}

TEST_F(MediaControlsOrientationLockDelegateTest, EnterFullscreenNoMetadata) {
  EXPECT_CALL(GetScreenOrientationController(), lock(_)).Times(0);

  SimulateEnterFullscreen();

  CheckStatePendingMetadata();
}

TEST_F(MediaControlsOrientationLockDelegateTest, LeaveFullscreenNoMetadata) {
  EXPECT_CALL(GetScreenOrientationController(), lock(_)).Times(0);
  EXPECT_CALL(GetScreenOrientationController(), MockUnlock()).Times(0);

  SimulateEnterFullscreen();
  // State set to PendingMetadata.
  SimulateExitFullscreen();

  CheckStatePendingFullscreen();
}

TEST_F(MediaControlsOrientationLockDelegateTest, EnterFullscreenWithMetadata) {
  SimulateVideoReadyState(HTMLMediaElement::kHaveMetadata);

  EXPECT_CALL(GetScreenOrientationController(), lock(_)).Times(1);
  EXPECT_FALSE(DelegateWillUnlockFullscreen());

  SimulateEnterFullscreen();

  EXPECT_TRUE(DelegateWillUnlockFullscreen());
  CheckStateMaybeLockedFullscreen();
}

TEST_F(MediaControlsOrientationLockDelegateTest, LeaveFullscreenWithMetadata) {
  SimulateVideoReadyState(HTMLMediaElement::kHaveMetadata);

  EXPECT_CALL(GetScreenOrientationController(), lock(_)).Times(1);
  EXPECT_CALL(GetScreenOrientationController(), MockUnlock()).Times(1);

  SimulateEnterFullscreen();
  // State set to MaybeLockedFullscreen.
  SimulateExitFullscreen();

  EXPECT_FALSE(DelegateWillUnlockFullscreen());
  CheckStatePendingFullscreen();
}

TEST_F(MediaControlsOrientationLockDelegateTest, EnterFullscreenAfterPageLock) {
  SimulateVideoReadyState(HTMLMediaElement::kHaveMetadata);
  SimulateOrientationLock();

  EXPECT_FALSE(DelegateWillUnlockFullscreen());
  EXPECT_CALL(GetScreenOrientationController(), lock(_)).Times(0);

  SimulateEnterFullscreen();

  EXPECT_FALSE(DelegateWillUnlockFullscreen());
  CheckStateMaybeLockedFullscreen();
}

TEST_F(MediaControlsOrientationLockDelegateTest, LeaveFullscreenAfterPageLock) {
  SimulateVideoReadyState(HTMLMediaElement::kHaveMetadata);
  SimulateOrientationLock();

  EXPECT_CALL(GetScreenOrientationController(), lock(_)).Times(0);
  EXPECT_CALL(GetScreenOrientationController(), MockUnlock()).Times(0);

  SimulateEnterFullscreen();
  // State set to MaybeLockedFullscreen.
  SimulateExitFullscreen();

  EXPECT_FALSE(DelegateWillUnlockFullscreen());
  CheckStatePendingFullscreen();
}

TEST_F(MediaControlsOrientationLockDelegateTest,
       ReceivedMetadataAfterExitingFullscreen) {
  EXPECT_CALL(GetScreenOrientationController(), lock(_)).Times(1);

  SimulateEnterFullscreen();
  // State set to PendingMetadata.

  // Set up the WebMediaPlayer instance.
  Video().SetSrc("http://example.com");
  testing::RunPendingTasks();

  SimulateVideoNetworkState(HTMLMediaElement::kNetworkIdle);
  SimulateVideoReadyState(HTMLMediaElement::kHaveMetadata);
  testing::RunPendingTasks();

  CheckStateMaybeLockedFullscreen();
}

TEST_F(MediaControlsOrientationLockDelegateTest, ReceivedMetadataLater) {
  EXPECT_CALL(GetScreenOrientationController(), lock(_)).Times(0);
  EXPECT_CALL(GetScreenOrientationController(), MockUnlock()).Times(0);

  SimulateEnterFullscreen();
  // State set to PendingMetadata.
  SimulateExitFullscreen();

  // Set up the WebMediaPlayer instance.
  Video().SetSrc("http://example.com");
  testing::RunPendingTasks();

  SimulateVideoNetworkState(HTMLMediaElement::kNetworkIdle);
  SimulateVideoReadyState(HTMLMediaElement::kHaveMetadata);
  testing::RunPendingTasks();

  CheckStatePendingFullscreen();
}

TEST_F(MediaControlsOrientationLockDelegateTest, ComputeOrientationLock) {
  // Set up the WebMediaPlayer instance.
  Video().SetSrc("http://example.com");
  testing::RunPendingTasks();

  SimulateVideoNetworkState(HTMLMediaElement::kNetworkIdle);
  SimulateVideoReadyState(HTMLMediaElement::kHaveMetadata);

  EXPECT_CALL(MockWebMediaPlayer(), NaturalSize())
      .Times(14)  // Each `computeOrientationLock` calls the method twice.
      .WillOnce(Return(WebSize(100, 50)))
      .WillOnce(Return(WebSize(100, 50)))
      .WillOnce(Return(WebSize(50, 100)))
      .WillOnce(Return(WebSize(50, 100)))
      .WillRepeatedly(Return(WebSize(100, 100)));

  // 100x50
  EXPECT_EQ(kWebScreenOrientationLockLandscape, ComputeOrientationLock());

  // 50x100
  EXPECT_EQ(kWebScreenOrientationLockPortrait, ComputeOrientationLock());

  // 100x100 has more subtilities, it depends on the current screen orientation.
  WebScreenInfo screen_info;
  screen_info.orientation_type = kWebScreenOrientationUndefined;
  EXPECT_CALL(ChromeClient(), GetScreenInfo())
      .Times(1)
      .WillOnce(Return(screen_info));
  EXPECT_EQ(kWebScreenOrientationLockLandscape, ComputeOrientationLock());

  screen_info.orientation_type = kWebScreenOrientationPortraitPrimary;
  EXPECT_CALL(ChromeClient(), GetScreenInfo())
      .Times(1)
      .WillOnce(Return(screen_info));
  EXPECT_EQ(kWebScreenOrientationLockPortrait, ComputeOrientationLock());

  screen_info.orientation_type = kWebScreenOrientationPortraitPrimary;
  EXPECT_CALL(ChromeClient(), GetScreenInfo())
      .Times(1)
      .WillOnce(Return(screen_info));
  EXPECT_EQ(kWebScreenOrientationLockPortrait, ComputeOrientationLock());

  screen_info.orientation_type = kWebScreenOrientationLandscapePrimary;
  EXPECT_CALL(ChromeClient(), GetScreenInfo())
      .Times(1)
      .WillOnce(Return(screen_info));
  EXPECT_EQ(kWebScreenOrientationLockLandscape, ComputeOrientationLock());

  screen_info.orientation_type = kWebScreenOrientationLandscapeSecondary;
  EXPECT_CALL(ChromeClient(), GetScreenInfo())
      .Times(1)
      .WillOnce(Return(screen_info));
  EXPECT_EQ(kWebScreenOrientationLockLandscape, ComputeOrientationLock());
}

}  // namespace blink

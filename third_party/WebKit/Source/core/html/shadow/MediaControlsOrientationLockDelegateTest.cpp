// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/shadow/MediaControlsOrientationLockDelegate.h"

#include "core/dom/Document.h"
#include "core/dom/DocumentUserGestureToken.h"
#include "core/dom/Fullscreen.h"
#include "core/frame/ScreenOrientationController.h"
#include "core/html/HTMLAudioElement.h"
#include "core/html/HTMLVideoElement.h"
#include "core/html/shadow/MediaControls.h"
#include "core/loader/EmptyClients.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/UserGestureIndicator.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/WebMediaPlayer.h"
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
  void onSuccess() override {}
  void onError(WebLockOrientationError) override {}
};

class MockVideoWebMediaPlayer : public WebMediaPlayer {
 public:
  void load(LoadType, const WebMediaPlayerSource&, CORSMode) override{};
  void play() override{};
  void pause() override{};
  bool supportsSave() const override { return false; };
  void seek(double seconds) override{};
  void setRate(double) override{};
  void setVolume(double) override{};
  WebTimeRanges buffered() const override { return WebTimeRanges(); };
  WebTimeRanges seekable() const override { return WebTimeRanges(); };
  void setSinkId(const WebString& sinkId,
                 const WebSecurityOrigin&,
                 WebSetSinkIdCallbacks*) override{};
  bool hasVideo() const override { return true; };
  bool hasAudio() const override { return false; };
  bool paused() const override { return false; };
  bool seeking() const override { return false; };
  double duration() const override { return 0.0; };
  double currentTime() const override { return 0.0; };
  NetworkState getNetworkState() const override { return NetworkStateEmpty; };
  ReadyState getReadyState() const override { return ReadyStateHaveNothing; };
  WebString getErrorMessage() override { return WebString(); };
  bool didLoadingProgress() override { return false; };
  bool hasSingleSecurityOrigin() const override { return true; };
  bool didPassCORSAccessCheck() const override { return true; };
  double mediaTimeForTimeValue(double timeValue) const override {
    return timeValue;
  };
  unsigned decodedFrameCount() const override { return 0; };
  unsigned droppedFrameCount() const override { return 0; };
  size_t audioDecodedByteCount() const override { return 0; };
  size_t videoDecodedByteCount() const override { return 0; };
  void paint(WebCanvas*, const WebRect&, PaintFlags&) override{};

  MOCK_CONST_METHOD0(naturalSize, WebSize());
};

class MockChromeClient : public EmptyChromeClient {
 public:
  MOCK_CONST_METHOD0(screenInfo, WebScreenInfo());
};

class StubLocalFrameClient : public EmptyLocalFrameClient {
 public:
  static StubLocalFrameClient* create() { return new StubLocalFrameClient; }

  std::unique_ptr<WebMediaPlayer> createWebMediaPlayer(
      HTMLMediaElement&,
      const WebMediaPlayerSource&,
      WebMediaPlayerClient*) override {
    return WTF::wrapUnique(new MockVideoWebMediaPlayer());
  }
};

class MockScreenOrientationController final
    : public ScreenOrientationController {
  WTF_MAKE_NONCOPYABLE(MockScreenOrientationController);

 public:
  static MockScreenOrientationController* provideTo(LocalFrame& frame) {
    MockScreenOrientationController* controller =
        new MockScreenOrientationController(frame);
    ScreenOrientationController::provideTo(frame, controller);
    return controller;
  }

  MOCK_METHOD1(lock, void(WebScreenOrientationLockType));
  MOCK_METHOD0(mockUnlock, void());

  DEFINE_INLINE_VIRTUAL_TRACE() { ScreenOrientationController::trace(visitor); }

 private:
  explicit MockScreenOrientationController(LocalFrame& frame)
      : ScreenOrientationController(frame) {}

  void lock(WebScreenOrientationLockType type,
            std::unique_ptr<WebLockOrientationCallback>) override {
    m_locked = true;
    lock(type);
  }

  void unlock() override {
    m_locked = false;
    mockUnlock();
  }

  bool maybeHasActiveLock() const override { return m_locked; }

  bool m_locked = false;
};

}  // anonymous namespace

class MediaControlsOrientationLockDelegateTest : public ::testing::Test {
 protected:
  void SetUp() override {
    m_previousVideoFullscreenOrientationLockValue =
        RuntimeEnabledFeatures::videoFullscreenOrientationLockEnabled();
    RuntimeEnabledFeatures::setVideoFullscreenOrientationLockEnabled(true);

    m_chromeClient = new MockChromeClient();

    Page::PageClients clients;
    fillWithEmptyClients(clients);
    clients.chromeClient = m_chromeClient.get();

    m_pageHolder = DummyPageHolder::create(IntSize(800, 600), &clients,
                                           StubLocalFrameClient::create());

    document().write("<body><video></body>");
    m_video = toHTMLVideoElement(*document().querySelector("video"));

    m_screenOrientationController =
        MockScreenOrientationController::provideTo(m_pageHolder->frame());
  }

  void TearDown() override {
    ::testing::Mock::VerifyAndClear(&screenOrientationController());

    RuntimeEnabledFeatures::setVideoFullscreenOrientationLockEnabled(
        m_previousVideoFullscreenOrientationLockValue);
  }

  static bool hasDelegate(const MediaControls& mediaControls) {
    return !!mediaControls.m_orientationLockDelegate;
  }

  void simulateEnterFullscreen() {
    UserGestureIndicator gesture(DocumentUserGestureToken::create(&document()));

    Fullscreen::requestFullscreen(video());
    Fullscreen::from(document()).didEnterFullscreen();
    testing::runPendingTasks();
  }

  void simulateExitFullscreen() {
    Fullscreen::exitFullscreen(document());
    Fullscreen::from(document()).didExitFullscreen();
    testing::runPendingTasks();
  }

  void simulateOrientationLock() {
    ScreenOrientationController* controller =
        ScreenOrientationController::from(*document().frame());
    controller->lock(WebScreenOrientationLockLandscape,
                     WTF::wrapUnique(new DummyScreenOrientationCallback));
    EXPECT_TRUE(controller->maybeHasActiveLock());
  }

  void simulateVideoReadyState(HTMLMediaElement::ReadyState state) {
    video().setReadyState(state);
  }

  void simulateVideoNetworkState(HTMLMediaElement::NetworkState state) {
    video().setNetworkState(state);
  }

  void checkStatePendingFullscreen() const {
    EXPECT_EQ(MediaControlsOrientationLockDelegate::State::PendingFullscreen,
              m_video->mediaControls()->m_orientationLockDelegate->m_state);
  }

  void checkStatePendingMetadata() const {
    EXPECT_EQ(MediaControlsOrientationLockDelegate::State::PendingMetadata,
              m_video->mediaControls()->m_orientationLockDelegate->m_state);
  }

  void checkStateMaybeLockedFullscreen() const {
    EXPECT_EQ(
        MediaControlsOrientationLockDelegate::State::MaybeLockedFullscreen,
        m_video->mediaControls()->m_orientationLockDelegate->m_state);
  }

  bool delegateWillUnlockFullscreen() const {
    return m_video->mediaControls()
        ->m_orientationLockDelegate->m_shouldUnlockOrientation;
  }

  WebScreenOrientationLockType computeOrientationLock() const {
    return m_video->mediaControls()
        ->m_orientationLockDelegate->computeOrientationLock();
  }

  MockChromeClient& chromeClient() const { return *m_chromeClient; }

  HTMLVideoElement& video() const { return *m_video; }
  Document& document() const { return m_pageHolder->document(); }
  MockScreenOrientationController& screenOrientationController() const {
    return *m_screenOrientationController;
  }
  MockVideoWebMediaPlayer& mockWebMediaPlayer() const {
    return *static_cast<MockVideoWebMediaPlayer*>(video().webMediaPlayer());
  }

 private:
  bool m_previousVideoFullscreenOrientationLockValue;
  std::unique_ptr<DummyPageHolder> m_pageHolder;
  Persistent<HTMLVideoElement> m_video;
  Persistent<MockScreenOrientationController> m_screenOrientationController;
  Persistent<MockChromeClient> m_chromeClient;
};

TEST_F(MediaControlsOrientationLockDelegateTest, DelegateRequiresFlag) {
  // Flag on by default.
  EXPECT_TRUE(hasDelegate(*video().mediaControls()));

  // Same with flag off.
  RuntimeEnabledFeatures::setVideoFullscreenOrientationLockEnabled(false);
  HTMLVideoElement* video = HTMLVideoElement::create(document());
  document().body()->appendChild(video);
  EXPECT_FALSE(hasDelegate(*video->mediaControls()));
}

TEST_F(MediaControlsOrientationLockDelegateTest, DelegateRequiresVideo) {
  HTMLAudioElement* audio = HTMLAudioElement::create(document());
  document().body()->appendChild(audio);
  EXPECT_FALSE(hasDelegate(*audio->mediaControls()));
}

TEST_F(MediaControlsOrientationLockDelegateTest, InitialState) {
  checkStatePendingFullscreen();
}

TEST_F(MediaControlsOrientationLockDelegateTest, EnterFullscreenNoMetadata) {
  EXPECT_CALL(screenOrientationController(), lock(_)).Times(0);

  simulateEnterFullscreen();

  checkStatePendingMetadata();
}

TEST_F(MediaControlsOrientationLockDelegateTest, LeaveFullscreenNoMetadata) {
  EXPECT_CALL(screenOrientationController(), lock(_)).Times(0);
  EXPECT_CALL(screenOrientationController(), mockUnlock()).Times(0);

  simulateEnterFullscreen();
  // State set to PendingMetadata.
  simulateExitFullscreen();

  checkStatePendingFullscreen();
}

TEST_F(MediaControlsOrientationLockDelegateTest, EnterFullscreenWithMetadata) {
  simulateVideoReadyState(HTMLMediaElement::kHaveMetadata);

  EXPECT_CALL(screenOrientationController(), lock(_)).Times(1);
  EXPECT_FALSE(delegateWillUnlockFullscreen());

  simulateEnterFullscreen();

  EXPECT_TRUE(delegateWillUnlockFullscreen());
  checkStateMaybeLockedFullscreen();
}

TEST_F(MediaControlsOrientationLockDelegateTest, LeaveFullscreenWithMetadata) {
  simulateVideoReadyState(HTMLMediaElement::kHaveMetadata);

  EXPECT_CALL(screenOrientationController(), lock(_)).Times(1);
  EXPECT_CALL(screenOrientationController(), mockUnlock()).Times(1);

  simulateEnterFullscreen();
  // State set to MaybeLockedFullscreen.
  simulateExitFullscreen();

  EXPECT_FALSE(delegateWillUnlockFullscreen());
  checkStatePendingFullscreen();
}

TEST_F(MediaControlsOrientationLockDelegateTest, EnterFullscreenAfterPageLock) {
  simulateVideoReadyState(HTMLMediaElement::kHaveMetadata);
  simulateOrientationLock();

  EXPECT_FALSE(delegateWillUnlockFullscreen());
  EXPECT_CALL(screenOrientationController(), lock(_)).Times(0);

  simulateEnterFullscreen();

  EXPECT_FALSE(delegateWillUnlockFullscreen());
  checkStateMaybeLockedFullscreen();
}

TEST_F(MediaControlsOrientationLockDelegateTest, LeaveFullscreenAfterPageLock) {
  simulateVideoReadyState(HTMLMediaElement::kHaveMetadata);
  simulateOrientationLock();

  EXPECT_CALL(screenOrientationController(), lock(_)).Times(0);
  EXPECT_CALL(screenOrientationController(), mockUnlock()).Times(0);

  simulateEnterFullscreen();
  // State set to MaybeLockedFullscreen.
  simulateExitFullscreen();

  EXPECT_FALSE(delegateWillUnlockFullscreen());
  checkStatePendingFullscreen();
}

TEST_F(MediaControlsOrientationLockDelegateTest,
       ReceivedMetadataAfterExitingFullscreen) {
  EXPECT_CALL(screenOrientationController(), lock(_)).Times(1);

  simulateEnterFullscreen();
  // State set to PendingMetadata.

  // Set up the WebMediaPlayer instance.
  video().setSrc("http://example.com");
  testing::runPendingTasks();

  simulateVideoNetworkState(HTMLMediaElement::kNetworkIdle);
  simulateVideoReadyState(HTMLMediaElement::kHaveMetadata);
  testing::runPendingTasks();

  checkStateMaybeLockedFullscreen();
}

TEST_F(MediaControlsOrientationLockDelegateTest, ReceivedMetadataLater) {
  EXPECT_CALL(screenOrientationController(), lock(_)).Times(0);
  EXPECT_CALL(screenOrientationController(), mockUnlock()).Times(0);

  simulateEnterFullscreen();
  // State set to PendingMetadata.
  simulateExitFullscreen();

  // Set up the WebMediaPlayer instance.
  video().setSrc("http://example.com");
  testing::runPendingTasks();

  simulateVideoNetworkState(HTMLMediaElement::kNetworkIdle);
  simulateVideoReadyState(HTMLMediaElement::kHaveMetadata);
  testing::runPendingTasks();

  checkStatePendingFullscreen();
}

TEST_F(MediaControlsOrientationLockDelegateTest, ComputeOrientationLock) {
  // Set up the WebMediaPlayer instance.
  video().setSrc("http://example.com");
  testing::runPendingTasks();

  simulateVideoNetworkState(HTMLMediaElement::kNetworkIdle);
  simulateVideoReadyState(HTMLMediaElement::kHaveMetadata);

  EXPECT_CALL(mockWebMediaPlayer(), naturalSize())
      .Times(14)  // Each `computeOrientationLock` calls the method twice.
      .WillOnce(Return(WebSize(100, 50)))
      .WillOnce(Return(WebSize(100, 50)))
      .WillOnce(Return(WebSize(50, 100)))
      .WillOnce(Return(WebSize(50, 100)))
      .WillRepeatedly(Return(WebSize(100, 100)));

  // 100x50
  EXPECT_EQ(WebScreenOrientationLockLandscape, computeOrientationLock());

  // 50x100
  EXPECT_EQ(WebScreenOrientationLockPortrait, computeOrientationLock());

  // 100x100 has more subtilities, it depends on the current screen orientation.
  WebScreenInfo screenInfo;
  screenInfo.orientationType = WebScreenOrientationUndefined;
  EXPECT_CALL(chromeClient(), screenInfo())
      .Times(1)
      .WillOnce(Return(screenInfo));
  EXPECT_EQ(WebScreenOrientationLockLandscape, computeOrientationLock());

  screenInfo.orientationType = WebScreenOrientationPortraitPrimary;
  EXPECT_CALL(chromeClient(), screenInfo())
      .Times(1)
      .WillOnce(Return(screenInfo));
  EXPECT_EQ(WebScreenOrientationLockPortrait, computeOrientationLock());

  screenInfo.orientationType = WebScreenOrientationPortraitPrimary;
  EXPECT_CALL(chromeClient(), screenInfo())
      .Times(1)
      .WillOnce(Return(screenInfo));
  EXPECT_EQ(WebScreenOrientationLockPortrait, computeOrientationLock());

  screenInfo.orientationType = WebScreenOrientationLandscapePrimary;
  EXPECT_CALL(chromeClient(), screenInfo())
      .Times(1)
      .WillOnce(Return(screenInfo));
  EXPECT_EQ(WebScreenOrientationLockLandscape, computeOrientationLock());

  screenInfo.orientationType = WebScreenOrientationLandscapeSecondary;
  EXPECT_CALL(chromeClient(), screenInfo())
      .Times(1)
      .WillOnce(Return(screenInfo));
  EXPECT_EQ(WebScreenOrientationLockLandscape, computeOrientationLock());
}

}  // namespace blink

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/media_controls/MediaControlsRotateToFullscreenDelegate.h"

#include "core/HTMLNames.h"
#include "core/css/CSSStyleDeclaration.h"
#include "core/dom/Document.h"
#include "core/dom/Fullscreen.h"
#include "core/dom/UserGestureIndicator.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLAudioElement.h"
#include "core/html/HTMLVideoElement.h"
#include "core/loader/EmptyClients.h"
#include "core/testing/DummyPageHolder.h"
#include "modules/media_controls/MediaControlsImpl.h"
#include "modules/screen_orientation/ScreenOrientationControllerImpl.h"
#include "platform/testing/EmptyWebMediaPlayer.h"
#include "platform/testing/UnitTestHelpers.h"
#include "platform/wtf/text/AtomicString.h"
#include "public/platform/WebSize.h"
#include "public/platform/modules/screen_orientation/WebScreenOrientationClient.h"
#include "public/platform/modules/screen_orientation/WebScreenOrientationType.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AtLeast;
using ::testing::Return;

namespace blink {

using namespace HTMLNames;

namespace {

class FakeWebScreenOrientationClient : public WebScreenOrientationClient {
 public:
  // WebScreenOrientationClient overrides:
  void LockOrientation(WebScreenOrientationLockType,
                       std::unique_ptr<WebLockOrientationCallback>) override {}
  void UnlockOrientation() override {}
};

class MockVideoWebMediaPlayer : public EmptyWebMediaPlayer {
 public:
  // EmptyWebMediaPlayer overrides:
  bool HasVideo() const override { return true; }

  MOCK_CONST_METHOD0(NaturalSize, WebSize());
};

class MockChromeClient : public EmptyChromeClient {
 public:
  // ChromeClient overrides:
  void InstallSupplements(LocalFrame& frame) override {
    EmptyChromeClient::InstallSupplements(frame);
    ScreenOrientationControllerImpl::ProvideTo(frame,
                                               &web_screen_orientation_client_);
  }
  void EnterFullscreen(LocalFrame& frame) override {
    Fullscreen::From(*frame.GetDocument()).DidEnterFullscreen();
  }
  void ExitFullscreen(LocalFrame& frame) override {
    Fullscreen::From(*frame.GetDocument()).DidExitFullscreen();
  }

  MOCK_CONST_METHOD0(GetScreenInfo, WebScreenInfo());

 private:
  FakeWebScreenOrientationClient web_screen_orientation_client_;
};

class StubLocalFrameClient : public EmptyLocalFrameClient {
 public:
  static StubLocalFrameClient* Create() { return new StubLocalFrameClient; }

  std::unique_ptr<WebMediaPlayer> CreateWebMediaPlayer(
      HTMLMediaElement&,
      const WebMediaPlayerSource&,
      WebMediaPlayerClient*) override {
    return WTF::MakeUnique<MockVideoWebMediaPlayer>();
  }
};

}  // anonymous namespace

class MediaControlsRotateToFullscreenDelegateTest : public ::testing::Test {
 protected:
  using SimpleOrientation =
      MediaControlsRotateToFullscreenDelegate::SimpleOrientation;

  void SetUp() override {
    previous_video_fullscreen_orientation_lock_value_ =
        RuntimeEnabledFeatures::VideoFullscreenOrientationLockEnabled();
    previous_video_rotate_to_fullscreen_value_ =
        RuntimeEnabledFeatures::VideoRotateToFullscreenEnabled();
    RuntimeEnabledFeatures::SetVideoFullscreenOrientationLockEnabled(true);
    RuntimeEnabledFeatures::SetVideoRotateToFullscreenEnabled(true);

    chrome_client_ = new MockChromeClient();

    Page::PageClients clients;
    FillWithEmptyClients(clients);
    clients.chrome_client = chrome_client_.Get();

    page_holder_ = DummyPageHolder::Create(IntSize(800, 600), &clients,
                                           StubLocalFrameClient::Create());

    video_ = HTMLVideoElement::Create(GetDocument());
    GetVideo().setAttribute(controlsAttr, g_empty_atom);
    // Most tests should call GetDocument().body()->AppendChild(&GetVideo());
    // This is not done automatically, so that tests control timing of `Attach`.
  }

  void TearDown() override {
    RuntimeEnabledFeatures::SetVideoFullscreenOrientationLockEnabled(
        previous_video_fullscreen_orientation_lock_value_);
    RuntimeEnabledFeatures::SetVideoRotateToFullscreenEnabled(
        previous_video_rotate_to_fullscreen_value_);
  }

  static bool HasDelegate(const MediaControls& media_controls) {
    return !!static_cast<const MediaControlsImpl*>(&media_controls)
                 ->rotate_to_fullscreen_delegate_;
  }

  void SimulateVideoReadyState(HTMLMediaElement::ReadyState state) {
    GetVideo().SetReadyState(state);
  }

  SimpleOrientation ObservedScreenOrientation() const {
    return GetMediaControls()
        .rotate_to_fullscreen_delegate_->current_screen_orientation_;
  }

  SimpleOrientation ComputeVideoOrientation() const {
    return GetMediaControls()
        .rotate_to_fullscreen_delegate_->ComputeVideoOrientation();
  }

  bool IsObservingVisibility() const {
    return GetMediaControls()
        .rotate_to_fullscreen_delegate_->visibility_observer_;
  }

  bool ObservedVisibility() const {
    return GetMediaControls().rotate_to_fullscreen_delegate_->is_visible_;
  }

  void DisableControls() {
    // If scripts are not enabled, controls will always be shown.
    page_holder_->GetFrame().GetSettings()->SetScriptEnabled(true);

    GetVideo().removeAttribute(controlsAttr);
  }

  void DispatchEvent(EventTarget& target, const AtomicString& type) {
    target.DispatchEvent(Event::Create(type));
  }

  void InitScreenAndVideo(WebScreenOrientationType initial_screen_orientation,
                          WebSize video_size);

  void PlayVideo();

  void UpdateVisibilityObserver() {
    // Let IntersectionObserver update.
    GetDocument().View()->UpdateAllLifecyclePhases();
    testing::RunPendingTasks();
  }

  void RotateTo(WebScreenOrientationType new_screen_orientation);

  MockChromeClient& GetChromeClient() const { return *chrome_client_; }
  LocalDOMWindow& GetWindow() const { return *GetDocument().domWindow(); }
  Document& GetDocument() const { return page_holder_->GetDocument(); }
  HTMLVideoElement& GetVideo() const { return *video_; }
  MediaControlsImpl& GetMediaControls() const {
    return *static_cast<MediaControlsImpl*>(GetVideo().GetMediaControls());
  }
  MockVideoWebMediaPlayer& GetWebMediaPlayer() const {
    return *static_cast<MockVideoWebMediaPlayer*>(
        GetVideo().GetWebMediaPlayer());
  }

 private:
  bool previous_video_fullscreen_orientation_lock_value_;
  bool previous_video_rotate_to_fullscreen_value_;
  Persistent<MockChromeClient> chrome_client_;
  std::unique_ptr<DummyPageHolder> page_holder_;
  Persistent<HTMLVideoElement> video_;
};

void MediaControlsRotateToFullscreenDelegateTest::InitScreenAndVideo(
    WebScreenOrientationType initial_screen_orientation,
    WebSize video_size) {
  // Set initial screen orientation (called by `Attach` during `AppendChild`).
  WebScreenInfo screen_info;
  screen_info.orientation_type = initial_screen_orientation;
  EXPECT_CALL(GetChromeClient(), GetScreenInfo())
      .Times(1)
      .WillOnce(Return(screen_info));

  // Set up the WebMediaPlayer instance.
  GetDocument().body()->AppendChild(&GetVideo());
  GetVideo().SetSrc("https://example.com");
  testing::RunPendingTasks();
  SimulateVideoReadyState(HTMLMediaElement::kHaveMetadata);

  // Set video size.
  EXPECT_CALL(GetWebMediaPlayer(), NaturalSize())
      .WillRepeatedly(Return(video_size));
}

void MediaControlsRotateToFullscreenDelegateTest::PlayVideo() {
  {
    UserGestureIndicator gesture(UserGestureToken::Create(&GetDocument()));
    GetVideo().Play();
  }
  testing::RunPendingTasks();
}

void MediaControlsRotateToFullscreenDelegateTest::RotateTo(
    WebScreenOrientationType new_screen_orientation) {
  WebScreenInfo screen_info;
  screen_info.orientation_type = new_screen_orientation;
  ::testing::Mock::VerifyAndClearExpectations(&GetChromeClient());
  EXPECT_CALL(GetChromeClient(), GetScreenInfo())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(screen_info));
  DispatchEvent(GetWindow(), EventTypeNames::orientationchange);
  testing::RunPendingTasks();
}

TEST_F(MediaControlsRotateToFullscreenDelegateTest, DelegateRequiresFlag) {
  // SetUp turns the flag on by default.
  GetDocument().body()->AppendChild(&GetVideo());
  EXPECT_TRUE(HasDelegate(GetMediaControls()));

  // No delegate when flag is off.
  RuntimeEnabledFeatures::SetVideoRotateToFullscreenEnabled(false);
  HTMLVideoElement* video = HTMLVideoElement::Create(GetDocument());
  GetDocument().body()->AppendChild(video);
  EXPECT_FALSE(HasDelegate(*video->GetMediaControls()));
}

TEST_F(MediaControlsRotateToFullscreenDelegateTest, DelegateRequiresVideo) {
  HTMLAudioElement* audio = HTMLAudioElement::Create(GetDocument());
  GetDocument().body()->AppendChild(audio);
  EXPECT_FALSE(HasDelegate(*audio->GetMediaControls()));
}

TEST_F(MediaControlsRotateToFullscreenDelegateTest, ComputeVideoOrientation) {
  // Set up the WebMediaPlayer instance.
  GetDocument().body()->AppendChild(&GetVideo());
  GetVideo().SetSrc("https://example.com");
  testing::RunPendingTasks();

  // Each `ComputeVideoOrientation` calls `NaturalSize` twice, except the first
  // one where the video is not yet ready.
  EXPECT_CALL(GetWebMediaPlayer(), NaturalSize())
      .Times(12)
      .WillOnce(Return(WebSize(400, 400)))
      .WillOnce(Return(WebSize(400, 400)))
      .WillOnce(Return(WebSize(300, 200)))
      .WillOnce(Return(WebSize(300, 200)))
      .WillOnce(Return(WebSize(200, 300)))
      .WillOnce(Return(WebSize(200, 300)))
      .WillOnce(Return(WebSize(300, 199)))
      .WillOnce(Return(WebSize(300, 199)))
      .WillOnce(Return(WebSize(199, 300)))
      .WillOnce(Return(WebSize(199, 300)))
      .WillOnce(Return(WebSize(0, 0)))
      .WillOnce(Return(WebSize(0, 0)));

  // Video is not yet ready.
  EXPECT_EQ(SimpleOrientation::kUnknown, ComputeVideoOrientation());

  SimulateVideoReadyState(HTMLMediaElement::kHaveMetadata);

  // 400x400 is square, which is currently treated as landscape.
  EXPECT_EQ(SimpleOrientation::kLandscape, ComputeVideoOrientation());
  // 300x200 is landscape.
  EXPECT_EQ(SimpleOrientation::kLandscape, ComputeVideoOrientation());
  // 200x300 is portrait.
  EXPECT_EQ(SimpleOrientation::kPortrait, ComputeVideoOrientation());
  // 300x199 is too small.
  EXPECT_EQ(SimpleOrientation::kUnknown, ComputeVideoOrientation());
  // 199x300 is too small.
  EXPECT_EQ(SimpleOrientation::kUnknown, ComputeVideoOrientation());
  // 0x0 is empty.
  EXPECT_EQ(SimpleOrientation::kUnknown, ComputeVideoOrientation());
}

TEST_F(MediaControlsRotateToFullscreenDelegateTest,
       OnlyObserveVisibilityWhenPlaying) {
  // Should not initially be observing visibility.
  GetDocument().body()->AppendChild(&GetVideo());
  EXPECT_FALSE(IsObservingVisibility());

  // Should start observing visibility when played.
  {
    UserGestureIndicator gesture(UserGestureToken::Create(&GetDocument()));
    GetVideo().Play();
  }
  testing::RunPendingTasks();
  EXPECT_TRUE(IsObservingVisibility());
  EXPECT_FALSE(ObservedVisibility());

  // Should have observed visibility once compositor updates.
  GetDocument().View()->UpdateAllLifecyclePhases();
  testing::RunPendingTasks();
  EXPECT_TRUE(ObservedVisibility());

  // Should stop observing visibility when paused.
  GetVideo().pause();
  testing::RunPendingTasks();
  EXPECT_FALSE(IsObservingVisibility());
  EXPECT_FALSE(ObservedVisibility());

  // Should resume observing visibility when playback resumes.
  {
    UserGestureIndicator gesture(UserGestureToken::Create(&GetDocument()));
    GetVideo().Play();
  }
  testing::RunPendingTasks();
  EXPECT_TRUE(IsObservingVisibility());
  EXPECT_FALSE(ObservedVisibility());

  // Should have observed visibility once compositor updates.
  GetDocument().View()->UpdateAllLifecyclePhases();
  testing::RunPendingTasks();
  EXPECT_TRUE(ObservedVisibility());
}

TEST_F(MediaControlsRotateToFullscreenDelegateTest,
       EnterSuccessPortraitToLandscape) {
  // Portrait screen, landscape video.
  InitScreenAndVideo(kWebScreenOrientationPortraitPrimary, WebSize(640, 480));
  EXPECT_EQ(SimpleOrientation::kPortrait, ObservedScreenOrientation());
  EXPECT_EQ(SimpleOrientation::kLandscape, ComputeVideoOrientation());

  // Play video.
  PlayVideo();
  UpdateVisibilityObserver();

  EXPECT_TRUE(ObservedVisibility());
  EXPECT_FALSE(GetVideo().IsFullscreen());

  // Rotate screen to landscape.
  RotateTo(kWebScreenOrientationLandscapePrimary);

  // Should enter fullscreen.
  EXPECT_TRUE(GetVideo().IsFullscreen());
}

TEST_F(MediaControlsRotateToFullscreenDelegateTest,
       EnterSuccessLandscapeToPortrait) {
  // Landscape screen, portrait video.
  InitScreenAndVideo(kWebScreenOrientationLandscapePrimary, WebSize(480, 640));
  EXPECT_EQ(SimpleOrientation::kLandscape, ObservedScreenOrientation());
  EXPECT_EQ(SimpleOrientation::kPortrait, ComputeVideoOrientation());

  // Play video.
  PlayVideo();
  UpdateVisibilityObserver();

  EXPECT_TRUE(ObservedVisibility());
  EXPECT_FALSE(GetVideo().IsFullscreen());

  // Rotate screen to portrait.
  RotateTo(kWebScreenOrientationPortraitPrimary);

  // Should enter fullscreen.
  EXPECT_TRUE(GetVideo().IsFullscreen());
}

TEST_F(MediaControlsRotateToFullscreenDelegateTest,
       EnterSuccessSquarePortraitToLandscape) {
  // Portrait screen, square video.
  InitScreenAndVideo(kWebScreenOrientationPortraitPrimary, WebSize(400, 400));
  EXPECT_EQ(SimpleOrientation::kPortrait, ObservedScreenOrientation());
  EXPECT_EQ(SimpleOrientation::kLandscape, ComputeVideoOrientation());

  // Play video.
  PlayVideo();
  UpdateVisibilityObserver();

  EXPECT_TRUE(ObservedVisibility());
  EXPECT_FALSE(GetVideo().IsFullscreen());

  // Rotate screen to landscape.
  RotateTo(kWebScreenOrientationLandscapePrimary);

  // Should enter fullscreen, since square videos are currently treated the same
  // as landscape videos.
  EXPECT_TRUE(GetVideo().IsFullscreen());
}

TEST_F(MediaControlsRotateToFullscreenDelegateTest, EnterFailWrongOrientation) {
  // Landscape screen, landscape video.
  InitScreenAndVideo(kWebScreenOrientationLandscapePrimary, WebSize(640, 480));
  EXPECT_EQ(SimpleOrientation::kLandscape, ObservedScreenOrientation());
  EXPECT_EQ(SimpleOrientation::kLandscape, ComputeVideoOrientation());

  // Play video.
  PlayVideo();
  UpdateVisibilityObserver();

  EXPECT_TRUE(ObservedVisibility());

  // Rotate screen to portrait.
  RotateTo(kWebScreenOrientationPortraitPrimary);

  // Should not enter fullscreen since the orientation that the device was
  // rotated to does not match the orientation of the video.
  EXPECT_FALSE(GetVideo().IsFullscreen());
}

TEST_F(MediaControlsRotateToFullscreenDelegateTest,
       EnterFailSquareWrongOrientation) {
  // Landscape screen, square video.
  InitScreenAndVideo(kWebScreenOrientationLandscapePrimary, WebSize(400, 400));
  EXPECT_EQ(SimpleOrientation::kLandscape, ObservedScreenOrientation());
  EXPECT_EQ(SimpleOrientation::kLandscape, ComputeVideoOrientation());

  // Play video.
  PlayVideo();
  UpdateVisibilityObserver();

  EXPECT_TRUE(ObservedVisibility());

  // Rotate screen to portrait.
  RotateTo(kWebScreenOrientationPortraitPrimary);

  // Should not enter fullscreen since square videos are treated as landscape,
  // so rotating to portrait does not match the orientation of the video.
  EXPECT_FALSE(GetVideo().IsFullscreen());
}

TEST_F(MediaControlsRotateToFullscreenDelegateTest, EnterFailNoControls) {
  DisableControls();

  // Portrait screen, landscape video.
  InitScreenAndVideo(kWebScreenOrientationPortraitPrimary, WebSize(640, 480));
  EXPECT_EQ(SimpleOrientation::kPortrait, ObservedScreenOrientation());
  EXPECT_EQ(SimpleOrientation::kLandscape, ComputeVideoOrientation());

  // Play video.
  PlayVideo();
  UpdateVisibilityObserver();

  EXPECT_TRUE(ObservedVisibility());

  // Rotate screen to landscape.
  RotateTo(kWebScreenOrientationLandscapePrimary);

  // Should not enter fullscreen since video has no controls.
  EXPECT_FALSE(GetVideo().IsFullscreen());
}

TEST_F(MediaControlsRotateToFullscreenDelegateTest, EnterFailPaused) {
  // Portrait screen, landscape video.
  InitScreenAndVideo(kWebScreenOrientationPortraitPrimary, WebSize(640, 480));
  EXPECT_EQ(SimpleOrientation::kPortrait, ObservedScreenOrientation());
  EXPECT_EQ(SimpleOrientation::kLandscape, ComputeVideoOrientation());

  EXPECT_FALSE(ObservedVisibility());

  UpdateVisibilityObserver();

  EXPECT_FALSE(ObservedVisibility());

  // Rotate screen to landscape.
  RotateTo(kWebScreenOrientationLandscapePrimary);

  // Should not enter fullscreen since video is paused.
  EXPECT_FALSE(GetVideo().IsFullscreen());
}

TEST_F(MediaControlsRotateToFullscreenDelegateTest, EnterFailHidden) {
  // Portrait screen, landscape video.
  InitScreenAndVideo(kWebScreenOrientationPortraitPrimary, WebSize(640, 480));
  EXPECT_EQ(SimpleOrientation::kPortrait, ObservedScreenOrientation());
  EXPECT_EQ(SimpleOrientation::kLandscape, ComputeVideoOrientation());

  // Play video.
  PlayVideo();
  UpdateVisibilityObserver();

  EXPECT_TRUE(ObservedVisibility());

  // Move video offscreen.
  GetDocument().body()->style()->setProperty("margin-top", "-999px", "",
                                             ASSERT_NO_EXCEPTION);

  UpdateVisibilityObserver();

  EXPECT_FALSE(ObservedVisibility());

  // Rotate screen to landscape.
  RotateTo(kWebScreenOrientationLandscapePrimary);

  // Should not enter fullscreen since video is not visible.
  EXPECT_FALSE(GetVideo().IsFullscreen());
}

TEST_F(MediaControlsRotateToFullscreenDelegateTest,
       EnterFail180DegreeRotation) {
  // Landscape screen, landscape video.
  InitScreenAndVideo(kWebScreenOrientationLandscapeSecondary,
                     WebSize(640, 480));
  EXPECT_EQ(SimpleOrientation::kLandscape, ObservedScreenOrientation());
  EXPECT_EQ(SimpleOrientation::kLandscape, ComputeVideoOrientation());

  // Play video.
  PlayVideo();
  UpdateVisibilityObserver();

  EXPECT_TRUE(ObservedVisibility());

  // Rotate screen 180 degrees to the opposite landscape (without passing via a
  // portrait orientation).
  RotateTo(kWebScreenOrientationLandscapePrimary);

  // Should not enter fullscreen since this is a 180 degree orientation.
  EXPECT_FALSE(GetVideo().IsFullscreen());
}

TEST_F(MediaControlsRotateToFullscreenDelegateTest, EnterFailSmall) {
  // Portrait screen, small landscape video.
  InitScreenAndVideo(kWebScreenOrientationPortraitPrimary, WebSize(300, 199));
  EXPECT_EQ(SimpleOrientation::kPortrait, ObservedScreenOrientation());
  EXPECT_EQ(SimpleOrientation::kUnknown, ComputeVideoOrientation());

  // Play video.
  PlayVideo();
  UpdateVisibilityObserver();

  EXPECT_TRUE(ObservedVisibility());

  // Rotate screen to landscape.
  RotateTo(kWebScreenOrientationLandscapePrimary);

  // Should not enter fullscreen since video is too small.
  EXPECT_FALSE(GetVideo().IsFullscreen());
}

TEST_F(MediaControlsRotateToFullscreenDelegateTest,
       EnterFailDocumentFullscreen) {
  // Portrait screen, landscape video.
  InitScreenAndVideo(kWebScreenOrientationPortraitPrimary, WebSize(640, 480));
  EXPECT_EQ(SimpleOrientation::kPortrait, ObservedScreenOrientation());
  EXPECT_EQ(SimpleOrientation::kLandscape, ComputeVideoOrientation());

  // Simulate the webpage requesting fullscreen on some other element than the
  // video (in this case document.body).
  {
    UserGestureIndicator gesture(UserGestureToken::Create(&GetDocument()));
    Fullscreen::RequestFullscreen(*GetDocument().body());
  }
  testing::RunPendingTasks();
  EXPECT_TRUE(Fullscreen::IsCurrentFullScreenElement(*GetDocument().body()));
  EXPECT_FALSE(GetVideo().IsFullscreen());

  // Play video.
  PlayVideo();
  UpdateVisibilityObserver();

  EXPECT_TRUE(ObservedVisibility());

  // Rotate screen to landscape.
  RotateTo(kWebScreenOrientationLandscapePrimary);

  // Should not enter fullscreen on video, since document is already fullscreen.
  EXPECT_TRUE(Fullscreen::IsCurrentFullScreenElement(*GetDocument().body()));
  EXPECT_FALSE(GetVideo().IsFullscreen());
}

TEST_F(MediaControlsRotateToFullscreenDelegateTest,
       ExitSuccessLandscapeFullscreenToPortraitInline) {
  // Landscape screen, landscape video.
  InitScreenAndVideo(kWebScreenOrientationLandscapePrimary, WebSize(640, 480));
  EXPECT_EQ(SimpleOrientation::kLandscape, ObservedScreenOrientation());
  EXPECT_EQ(SimpleOrientation::kLandscape, ComputeVideoOrientation());

  // Start in fullscreen.
  {
    UserGestureIndicator gesture(UserGestureToken::Create(&GetDocument()));
    GetMediaControls().EnterFullscreen();
  }
  // n.b. omit to call Fullscreen::From(GetDocument()).DidEnterFullscreen() so
  // that MediaControlsOrientationLockDelegate doesn't trigger, which avoids
  // having to create deviceorientation events here to unlock it again.
  testing::RunPendingTasks();
  EXPECT_TRUE(GetVideo().IsFullscreen());

  // Leave video paused (playing is not a requirement to exit fullscreen).
  EXPECT_TRUE(GetVideo().paused());
  EXPECT_FALSE(ObservedVisibility());

  // Rotate screen to portrait. This relies on the screen orientation not being
  // locked by MediaControlsOrientationLockDelegate (which has its own tests).
  RotateTo(kWebScreenOrientationPortraitPrimary);

  // Should exit fullscreen.
  EXPECT_FALSE(GetVideo().IsFullscreen());
}

TEST_F(MediaControlsRotateToFullscreenDelegateTest,
       ExitSuccessPortraitFullscreenToLandscapeInline) {
  // Portrait screen, portrait video.
  InitScreenAndVideo(kWebScreenOrientationPortraitPrimary, WebSize(480, 640));
  EXPECT_EQ(SimpleOrientation::kPortrait, ObservedScreenOrientation());
  EXPECT_EQ(SimpleOrientation::kPortrait, ComputeVideoOrientation());

  // Start in fullscreen.
  {
    UserGestureIndicator gesture(UserGestureToken::Create(&GetDocument()));
    GetMediaControls().EnterFullscreen();
  }
  // n.b. omit to call Fullscreen::From(GetDocument()).DidEnterFullscreen() so
  // that MediaControlsOrientationLockDelegate doesn't trigger, which avoids
  // having to create deviceorientation events here to unlock it again.
  testing::RunPendingTasks();
  EXPECT_TRUE(GetVideo().IsFullscreen());

  // Leave video paused (playing is not a requirement to exit fullscreen).
  EXPECT_TRUE(GetVideo().paused());
  EXPECT_FALSE(ObservedVisibility());

  // Rotate screen to landscape. This relies on the screen orientation not being
  // locked by MediaControlsOrientationLockDelegate (which has its own tests).
  RotateTo(kWebScreenOrientationLandscapePrimary);

  // Should exit fullscreen.
  EXPECT_FALSE(GetVideo().IsFullscreen());
}

TEST_F(MediaControlsRotateToFullscreenDelegateTest,
       ExitFailDocumentFullscreen) {
  // Landscape screen, landscape video.
  InitScreenAndVideo(kWebScreenOrientationLandscapePrimary, WebSize(640, 480));
  EXPECT_EQ(SimpleOrientation::kLandscape, ObservedScreenOrientation());
  EXPECT_EQ(SimpleOrientation::kLandscape, ComputeVideoOrientation());

  // Simulate the webpage requesting fullscreen on some other element than the
  // video (in this case document.body).
  {
    UserGestureIndicator gesture(UserGestureToken::Create(&GetDocument()));
    Fullscreen::RequestFullscreen(*GetDocument().body());
  }
  testing::RunPendingTasks();
  EXPECT_TRUE(Fullscreen::IsCurrentFullScreenElement(*GetDocument().body()));
  EXPECT_FALSE(GetVideo().IsFullscreen());

  // Leave video paused (playing is not a requirement to exit fullscreen).
  EXPECT_TRUE(GetVideo().paused());
  EXPECT_FALSE(ObservedVisibility());

  // Rotate screen to portrait.
  RotateTo(kWebScreenOrientationPortraitPrimary);

  // Should not exit fullscreen, since video was not the fullscreen element.
  EXPECT_TRUE(Fullscreen::IsCurrentFullScreenElement(*GetDocument().body()));
  EXPECT_FALSE(GetVideo().IsFullscreen());
}

}  // namespace blink

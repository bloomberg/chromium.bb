// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/non_touch/media_controls_non_touch_impl.h"

#include <memory>

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/media/html_media_test_helper.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"
#include "third_party/blink/renderer/platform/testing/empty_web_media_player.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {

class MockWebMediaPlayerForNonTouchImpl : public EmptyWebMediaPlayer {
 public:
  WebTimeRanges Seekable() const override { return seekable_; }
  bool HasVideo() const override { return true; }

  WebTimeRanges seekable_;
};

class MockChromeClientForNonTouchImpl : public EmptyChromeClient {
 public:
  explicit MockChromeClientForNonTouchImpl()
      : orientation_(kWebScreenOrientationPortraitPrimary) {}

  WebScreenInfo GetScreenInfo() const override {
    WebScreenInfo screen_info;
    screen_info.orientation_type = orientation_;
    return screen_info;
  }

  void SetOrientation(WebScreenOrientationType orientation_type) {
    orientation_ = orientation_type;
  }

 private:
  WebScreenOrientationType orientation_;
};

class MediaControlsNonTouchImplTest : public PageTestBase {
 protected:
  void SetUp() override { InitializePage(); }

  void InitializePage() {
    Page::PageClients clients;
    FillWithEmptyClients(clients);
    chrome_client_ = MakeGarbageCollected<MockChromeClientForNonTouchImpl>();
    clients.chrome_client = chrome_client_;
    SetupPageWithClients(
        &clients, test::MediaStubLocalFrameClient::Create(
                      std::make_unique<MockWebMediaPlayerForNonTouchImpl>()));

    GetDocument().write("<video>");
    HTMLMediaElement& video =
        ToHTMLVideoElement(*GetDocument().QuerySelector("video"));
    media_controls_ = MediaControlsNonTouchImpl::Create(
        video, video.EnsureUserAgentShadowRoot());
  }

  MediaControlsNonTouchImpl& MediaControls() { return *media_controls_; }
  HTMLMediaElement& MediaElement() { return MediaControls().MediaElement(); }

  MockWebMediaPlayerForNonTouchImpl* WebMediaPlayer() {
    return static_cast<MockWebMediaPlayerForNonTouchImpl*>(
        MediaElement().GetWebMediaPlayer());
  }

  void SimulateKeydownEvent(Element& element, int key_code) {
    KeyboardEventInit* keyboard_event_init = KeyboardEventInit::Create();
    keyboard_event_init->setKeyCode(key_code);

    Event* keyboard_event =
        MakeGarbageCollected<KeyboardEvent>("keydown", keyboard_event_init);
    element.DispatchEvent(*keyboard_event);
  }

  void LoadMediaWithDuration(double duration) {
    MediaElement().SetSrc("https://example.com/foo.mp4");
    test::RunPendingTasks();
    WebTimeRange time_range(0.0, duration);
    WebMediaPlayer()->seekable_.Assign(&time_range, 1);
    MediaElement().DurationChanged(duration, false /* requestSeek */);
  }

  void SetScreenOrientation(WebScreenOrientationType orientation_type) {
    chrome_client_->SetOrientation(orientation_type);
  }

  void CheckControlKeys(int seek_forward_key,
                        int seek_backward_key,
                        int volume_up_key,
                        int volume_down_key) {
    const int kNumberOfSecondsToJump = 10;
    const double kVolumeToChange = 0.05;
    const int initTime = 10;
    const double initVolume = 0.5;
    MediaElement().setCurrentTime(initTime);
    MediaElement().setVolume(initVolume);

    SimulateKeydownEvent(MediaElement(), seek_forward_key);
    ASSERT_EQ(MediaElement().currentTime(), initTime + kNumberOfSecondsToJump);

    SimulateKeydownEvent(MediaElement(), seek_backward_key);
    ASSERT_EQ(MediaElement().currentTime(), initTime);

    SimulateKeydownEvent(MediaElement(), volume_up_key);
    ASSERT_EQ(MediaElement().volume(), initVolume + kVolumeToChange);

    SimulateKeydownEvent(MediaElement(), volume_down_key);
    ASSERT_EQ(MediaElement().volume(), initVolume);
  }

 private:
  Persistent<MediaControlsNonTouchImpl> media_controls_;
  Persistent<MockChromeClientForNonTouchImpl> chrome_client_;
};

TEST_F(MediaControlsNonTouchImplTest, PlayPause) {
  MediaElement().SetFocused(true, WebFocusType::kWebFocusTypeNone);
  MediaElement().Play();
  ASSERT_FALSE(MediaElement().paused());

  // Press center key and video should be paused.
  SimulateKeydownEvent(MediaElement(), VKEY_RETURN);
  ASSERT_TRUE(MediaElement().paused());

  // Press center key and video should be played.
  SimulateKeydownEvent(MediaElement(), VKEY_RETURN);
  ASSERT_FALSE(MediaElement().paused());
}

TEST_F(MediaControlsNonTouchImplTest, HandlesOrientationForArrowInput) {
  MediaElement().SetFocused(true, WebFocusType::kWebFocusTypeNone);

  SetScreenOrientation(kWebScreenOrientationPortraitPrimary);
  CheckControlKeys(VKEY_RIGHT, VKEY_LEFT, VKEY_UP, VKEY_DOWN);

  SetScreenOrientation(kWebScreenOrientationLandscapePrimary);
  CheckControlKeys(VKEY_DOWN, VKEY_UP, VKEY_RIGHT, VKEY_LEFT);

  SetScreenOrientation(kWebScreenOrientationPortraitSecondary);
  CheckControlKeys(VKEY_LEFT, VKEY_RIGHT, VKEY_DOWN, VKEY_UP);

  SetScreenOrientation(kWebScreenOrientationLandscapeSecondary);
  CheckControlKeys(VKEY_UP, VKEY_DOWN, VKEY_LEFT, VKEY_RIGHT);
}

TEST_F(MediaControlsNonTouchImplTest, ArrowInputEdgeCaseHandling) {
  const double duration = 100;

  LoadMediaWithDuration(duration);
  MediaElement().SetFocused(true, WebFocusType::kWebFocusTypeNone);

  // Seek backward at low current time
  MediaElement().setCurrentTime(1);
  SimulateKeydownEvent(MediaElement(), VKEY_LEFT);
  ASSERT_EQ(MediaElement().currentTime(), 0);

  // Seek forward at high current time
  MediaElement().setCurrentTime(duration - 1);
  SimulateKeydownEvent(MediaElement(), VKEY_RIGHT);
  ASSERT_EQ(MediaElement().currentTime(), duration);

  // Volume down at low volume
  MediaElement().setVolume(0.01);
  SimulateKeydownEvent(MediaElement(), VKEY_DOWN);
  ASSERT_EQ(MediaElement().volume(), 0);

  // Volume up at high volume
  MediaElement().setVolume(0.99);
  SimulateKeydownEvent(MediaElement(), VKEY_UP);
  ASSERT_EQ(MediaElement().volume(), 1);
}

}  // namespace

}  // namespace blink

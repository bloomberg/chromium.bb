// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_controls/touchless/media_controls_touchless_impl.h"

#include <memory>

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/dom/dom_token_list.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/media/html_media_test_helper.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/html/time_ranges.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"
#include "third_party/blink/renderer/platform/testing/empty_web_media_player.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {

class MockWebMediaPlayerForTouchlessImpl : public EmptyWebMediaPlayer {
 public:
  WebTimeRanges Seekable() const override { return seekable_; }
  bool HasVideo() const override { return true; }
  WebTimeRanges Buffered() const override { return buffered_; }

  WebTimeRanges buffered_;
  WebTimeRanges seekable_;
};

class MockChromeClientForTouchlessImpl : public EmptyChromeClient {
 public:
  explicit MockChromeClientForTouchlessImpl()
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

class MediaControlsTouchlessImplTest : public PageTestBase {
 protected:
  void SetUp() override { InitializePage(); }

  void InitializePage() {
    Page::PageClients clients;
    FillWithEmptyClients(clients);
    chrome_client_ = MakeGarbageCollected<MockChromeClientForTouchlessImpl>();
    clients.chrome_client = chrome_client_;
    SetupPageWithClients(
        &clients, MakeGarbageCollected<test::MediaStubLocalFrameClient>(
                      std::make_unique<MockWebMediaPlayerForTouchlessImpl>()));

    GetDocument().write("<video controls>");
    HTMLMediaElement& video =
        ToHTMLVideoElement(*GetDocument().QuerySelector("video"));
    media_controls_ =
        static_cast<MediaControlsTouchlessImpl*>(video.GetMediaControls());
  }

  MediaControlsTouchlessImpl& MediaControls() { return *media_controls_; }
  HTMLMediaElement& MediaElement() { return MediaControls().MediaElement(); }

  MockWebMediaPlayerForTouchlessImpl* WebMediaPlayer() {
    return static_cast<MockWebMediaPlayerForTouchlessImpl*>(
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

  void SetBufferedRange(double end) {
    WebTimeRange time_range(0.0, end);
    WebMediaPlayer()->buffered_.Assign(&time_range, 1);
  }

  bool IsControlsVisible() {
    return !MediaControls().classList().contains("transparent");
  }

  Element* GetControlByShadowPseudoId(const char* shadow_pseudo_id) {
    for (Element& element : ElementTraversal::DescendantsOf(MediaControls())) {
      if (element.ShadowPseudoId() == shadow_pseudo_id)
        return &element;
    }
    return nullptr;
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
  Persistent<MediaControlsTouchlessImpl> media_controls_;
  Persistent<MockChromeClientForTouchlessImpl> chrome_client_;
};

class MediaControlsTouchlessImplTestWithMockScheduler
    : public MediaControlsTouchlessImplTest {
 public:
  MediaControlsTouchlessImplTestWithMockScheduler() { EnablePlatform(); }

 protected:
  void SetUp() override {
    platform()->AdvanceClockSeconds(1);
    MediaControlsTouchlessImplTest::SetUp();
  }
};

TEST_F(MediaControlsTouchlessImplTest, PlayPause) {
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

TEST_F(MediaControlsTouchlessImplTest, HandlesOrientationForArrowInput) {
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

TEST_F(MediaControlsTouchlessImplTest, ArrowInputEdgeCaseHandling) {
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

TEST_F(MediaControlsTouchlessImplTest, PlayPauseIcon) {
  MediaElement().SetFocused(true, WebFocusType::kWebFocusTypeNone);

  Element* play_button = GetControlByShadowPseudoId(
      "-internal-media-controls-touchless-play-button");
  ASSERT_NE(nullptr, play_button);

  MediaElement().pause();

  MediaElement().Play();
  test::RunPendingTasks();
  ASSERT_TRUE(play_button->classList().contains("playing"));
  ASSERT_FALSE(play_button->classList().contains("paused"));

  MediaElement().pause();
  test::RunPendingTasks();
  ASSERT_TRUE(play_button->classList().contains("paused"));
  ASSERT_FALSE(play_button->classList().contains("playing"));
}

TEST_F(MediaControlsTouchlessImplTest, ProgressBar) {
  const double duration = 100.0;
  const double buffered = 60.0;
  const double current_time = 15.0;

  LoadMediaWithDuration(duration);
  SetBufferedRange(buffered);
  MediaElement().setCurrentTime(current_time);
  test::RunPendingTasks();

  MediaElement().DispatchEvent(*Event::Create(event_type_names::kTimeupdate));

  Element* timeline =
      GetControlByShadowPseudoId("-internal-media-controls-touchless-timeline");
  Element* progress_bar = GetControlByShadowPseudoId(
      "-internal-media-controls-touchless-timeline-progress");
  Element* loaded_bar = GetControlByShadowPseudoId(
      "-internal-media-controls-touchless-timeline-loaded");

  ASSERT_NE(nullptr, timeline);
  ASSERT_NE(nullptr, progress_bar);
  ASSERT_NE(nullptr, loaded_bar);

  double timeline_width = timeline->getBoundingClientRect()->width();
  double progress_bar_width = progress_bar->getBoundingClientRect()->width();
  double loaded_bar_width = loaded_bar->getBoundingClientRect()->width();
  ASSERT_GT(timeline_width, 0);

  EXPECT_DOUBLE_EQ(buffered / duration, loaded_bar_width / timeline_width);
  EXPECT_DOUBLE_EQ(current_time / buffered,
                   progress_bar_width / loaded_bar_width);
}

TEST_F(MediaControlsTouchlessImplTest, TimeDisplay) {
  const double duration = 4000;
  const double current_time = 3599;
  const char expect_display[] = "59:59 / 1:06:40";

  Element* time_display = GetControlByShadowPseudoId(
      "-internal-media-controls-touchless-time-display");

  EXPECT_EQ(time_display->InnerHTMLAsString(), "0:00 / 0:00");

  LoadMediaWithDuration(duration);
  MediaElement().setCurrentTime(current_time);
  test::RunPendingTasks();
  MediaElement().DispatchEvent(*Event::Create(event_type_names::kTimeupdate));

  EXPECT_EQ(time_display->InnerHTMLAsString(), expect_display);
}

TEST_F(MediaControlsTouchlessImplTestWithMockScheduler, ControlsShowAndHide) {
  // Controls should starts hidden.
  ASSERT_FALSE(IsControlsVisible());

  // Controls should show when focus in.
  MediaElement().SetFocused(true, WebFocusType::kWebFocusTypeNone);
  MediaElement().DispatchEvent(*Event::Create(event_type_names::kFocusin));
  ASSERT_TRUE(IsControlsVisible());

  // Controls should hide after 3 seconds.
  platform()->RunForPeriodSeconds(2.99);
  ASSERT_TRUE(IsControlsVisible());
  platform()->RunForPeriodSeconds(0.01);
  ASSERT_FALSE(IsControlsVisible());

  // Controls should show upon pressing return key.
  SimulateKeydownEvent(MediaElement(), VKEY_RETURN);
  ASSERT_TRUE(IsControlsVisible());

  // Controls should not disappear after 2 seconds.
  platform()->RunForPeriodSeconds(2);
  ASSERT_TRUE(IsControlsVisible());

  // Pressing return key again should reset hiding timer.
  SimulateKeydownEvent(MediaElement(), VKEY_RETURN);
  platform()->RunForPeriodSeconds(2);
  ASSERT_TRUE(IsControlsVisible());

  // Controls should hide 3 seconds after last key press.
  platform()->RunForPeriodSeconds(1);
  ASSERT_FALSE(IsControlsVisible());
}

}  // namespace

}  // namespace blink

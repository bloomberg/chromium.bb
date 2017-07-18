// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/media_controls/MediaControlsImpl.h"

#include <limits>
#include <memory>

#include "build/build_config.h"
#include "core/HTMLNames.h"
#include "core/css/StylePropertySet.h"
#include "core/dom/Document.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/StyleEngine.h"
#include "core/events/Event.h"
#include "core/frame/Settings.h"
#include "core/geometry/DOMRect.h"
#include "core/html/HTMLElement.h"
#include "core/html/HTMLVideoElement.h"
#include "core/input/EventHandler.h"
#include "core/layout/LayoutObject.h"
#include "core/loader/EmptyClients.h"
#include "core/testing/DummyPageHolder.h"
#include "modules/media_controls/elements/MediaControlCurrentTimeDisplayElement.h"
#include "modules/media_controls/elements/MediaControlDownloadButtonElement.h"
#include "modules/media_controls/elements/MediaControlTimelineElement.h"
#include "modules/media_controls/elements/MediaControlVolumeSliderElement.h"
#include "modules/remoteplayback/HTMLMediaElementRemotePlayback.h"
#include "modules/remoteplayback/RemotePlayback.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/heap/Handle.h"
#include "platform/testing/EmptyWebMediaPlayer.h"
#include "platform/testing/HistogramTester.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/WebMouseEvent.h"
#include "public/platform/WebScreenInfo.h"
#include "public/platform/WebSize.h"
#include "public/platform/modules/remoteplayback/WebRemotePlaybackAvailability.h"
#include "public/platform/modules/remoteplayback/WebRemotePlaybackClient.h"
#include "testing/gtest/include/gtest/gtest.h"

// The MediaTimelineWidths histogram suffix expected to be encountered in these
// tests. Depends on the OS, since Android sizes its timeline differently.
#if defined(OS_ANDROID)
#define TIMELINE_W "80_127"
#else
#define TIMELINE_W "128_255"
#endif

namespace blink {

namespace {

class MockChromeClientForImpl : public EmptyChromeClient {
 public:
  // EmptyChromeClient overrides:
  WebScreenInfo GetScreenInfo() const override {
    WebScreenInfo screen_info;
    screen_info.orientation_type = kWebScreenOrientationLandscapePrimary;
    return screen_info;
  }
};

class MockWebMediaPlayerForImpl : public EmptyWebMediaPlayer {
 public:
  // WebMediaPlayer overrides:
  WebTimeRanges Seekable() const override { return seekable_; }
  bool HasVideo() const override { return true; }

  WebTimeRanges seekable_;
};

class MockLayoutObject : public LayoutObject {
 public:
  MockLayoutObject(Node* node) : LayoutObject(node) {}

  const char* GetName() const override { return "MockLayoutObject"; }
  void UpdateLayout() override {}
  FloatRect LocalBoundingBoxRectForAccessibility() const override {
    return FloatRect();
  }
};

class StubLocalFrameClientForImpl : public EmptyLocalFrameClient {
 public:
  static StubLocalFrameClientForImpl* Create() {
    return new StubLocalFrameClientForImpl;
  }

  std::unique_ptr<WebMediaPlayer> CreateWebMediaPlayer(
      HTMLMediaElement&,
      const WebMediaPlayerSource&,
      WebMediaPlayerClient*) override {
    return WTF::WrapUnique(new MockWebMediaPlayerForImpl);
  }

  WebRemotePlaybackClient* CreateWebRemotePlaybackClient(
      HTMLMediaElement& element) override {
    return HTMLMediaElementRemotePlayback::remote(element);
  }
};

Element* GetElementByShadowPseudoId(Node& root_node,
                                    const char* shadow_pseudo_id) {
  for (Element& element : ElementTraversal::DescendantsOf(root_node)) {
    if (element.ShadowPseudoId() == shadow_pseudo_id)
      return &element;
  }
  return nullptr;
}

bool IsElementVisible(Element& element) {
  const StylePropertySet* inline_style = element.InlineStyle();

  if (!inline_style)
    return true;

  if (inline_style->GetPropertyValue(CSSPropertyDisplay) == "none")
    return false;

  if (inline_style->HasProperty(CSSPropertyOpacity) &&
      inline_style->GetPropertyValue(CSSPropertyOpacity).ToDouble() == 0.0) {
    return false;
  }

  if (inline_style->GetPropertyValue(CSSPropertyVisibility) == "hidden")
    return false;

  if (Element* parent = element.parentElement())
    return IsElementVisible(*parent);

  return true;
}

// This must match MediaControlDownloadButtonElement::DownloadActionMetrics.
enum DownloadActionMetrics {
  kShown = 0,
  kClicked,
  kCount  // Keep last.
};

}  // namespace

class MediaControlsImplTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    // Enable the cast overlay button as this is enabled by default.
    RuntimeEnabledFeatures::SetMediaCastOverlayButtonEnabled(true);

    InitializePage();
  }

  void InitializePage() {
    Page::PageClients clients;
    FillWithEmptyClients(clients);
    clients.chrome_client = new MockChromeClientForImpl();
    page_holder_ = DummyPageHolder::Create(
        IntSize(800, 600), &clients, StubLocalFrameClientForImpl::Create());

    GetDocument().write("<video>");
    HTMLVideoElement& video =
        toHTMLVideoElement(*GetDocument().QuerySelector("video"));
    media_controls_ = static_cast<MediaControlsImpl*>(video.GetMediaControls());

    // If scripts are not enabled, controls will always be shown.
    page_holder_->GetFrame().GetSettings()->SetScriptEnabled(true);
  }

  void SimulateRouteAvailable() {
    media_controls_->MediaElement().RemoteRouteAvailabilityChanged(
        WebRemotePlaybackAvailability::kDeviceAvailable);
  }

  void EnsureSizing() {
    // Fire the size-change callback to ensure that the controls have
    // been properly notified of the video size.
    media_controls_->NotifyElementSizeChanged(
        media_controls_->MediaElement().getBoundingClientRect());
  }

  void SimulateHideMediaControlsTimerFired() {
    media_controls_->HideMediaControlsTimerFired(nullptr);
  }

  void SimulateLoadedMetadata() { media_controls_->OnLoadedMetadata(); }

  MediaControlsImpl& MediaControls() { return *media_controls_; }
  MediaControlVolumeSliderElement* VolumeSliderElement() const {
    return media_controls_->volume_slider_;
  }
  MediaControlTimelineElement* TimelineElement() const {
    return media_controls_->timeline_;
  }
  MediaControlCurrentTimeDisplayElement* GetCurrentTimeDisplayElement() const {
    return media_controls_->current_time_display_;
  }
  MockWebMediaPlayerForImpl* WebMediaPlayer() {
    return static_cast<MockWebMediaPlayerForImpl*>(
        MediaControls().MediaElement().GetWebMediaPlayer());
  }
  Document& GetDocument() { return page_holder_->GetDocument(); }

  HistogramTester& GetHistogramTester() { return histogram_tester_; }

  void LoadMediaWithDuration(double duration) {
    MediaControls().MediaElement().SetSrc("https://example.com/foo.mp4");
    testing::RunPendingTasks();
    WebTimeRange time_range(0.0, duration);
    WebMediaPlayer()->seekable_.Assign(&time_range, 1);
    MediaControls().MediaElement().DurationChanged(duration,
                                                   false /* requestSeek */);
    SimulateLoadedMetadata();
  }

  void SetReady() {
    MediaControls().MediaElement().SetReadyState(
        HTMLMediaElement::kHaveEnoughData);
  }

  void MouseDownAt(WebFloatPoint pos);
  void MouseMoveTo(WebFloatPoint pos);
  void MouseUpAt(WebFloatPoint pos);

  bool HasAvailabilityCallbacks(RemotePlayback* remote_playback) {
    return !remote_playback->availability_callbacks_.IsEmpty();
  }

 private:
  std::unique_ptr<DummyPageHolder> page_holder_;
  Persistent<MediaControlsImpl> media_controls_;
  HistogramTester histogram_tester_;
};

void MediaControlsImplTest::MouseDownAt(WebFloatPoint pos) {
  WebMouseEvent mouse_down_event(WebInputEvent::kMouseDown,
                                 pos /* client pos */, pos /* screen pos */,
                                 WebPointerProperties::Button::kLeft, 1,
                                 WebInputEvent::Modifiers::kLeftButtonDown,
                                 WebInputEvent::kTimeStampForTesting);
  mouse_down_event.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleMousePressEvent(
      mouse_down_event);
}

void MediaControlsImplTest::MouseMoveTo(WebFloatPoint pos) {
  WebMouseEvent mouse_move_event(WebInputEvent::kMouseMove,
                                 pos /* client pos */, pos /* screen pos */,
                                 WebPointerProperties::Button::kLeft, 1,
                                 WebInputEvent::Modifiers::kLeftButtonDown,
                                 WebInputEvent::kTimeStampForTesting);
  mouse_move_event.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleMouseMoveEvent(
      mouse_move_event, {});
}

void MediaControlsImplTest::MouseUpAt(WebFloatPoint pos) {
  WebMouseEvent mouse_up_event(
      WebMouseEvent::kMouseUp, pos /* client pos */, pos /* screen pos */,
      WebPointerProperties::Button::kLeft, 1, WebInputEvent::kNoModifiers,
      WebInputEvent::kTimeStampForTesting);
  mouse_up_event.SetFrameScale(1);
  GetDocument().GetFrame()->GetEventHandler().HandleMouseReleaseEvent(
      mouse_up_event);
}

TEST_F(MediaControlsImplTest, HideAndShow) {
  MediaControls().MediaElement().SetBooleanAttribute(HTMLNames::controlsAttr,
                                                     true);

  Element* panel = GetElementByShadowPseudoId(MediaControls(),
                                              "-webkit-media-controls-panel");
  ASSERT_NE(nullptr, panel);

  ASSERT_TRUE(IsElementVisible(*panel));
  MediaControls().Hide();
  ASSERT_FALSE(IsElementVisible(*panel));
  MediaControls().MaybeShow();
  ASSERT_TRUE(IsElementVisible(*panel));
}

TEST_F(MediaControlsImplTest, Reset) {
  MediaControls().MediaElement().SetBooleanAttribute(HTMLNames::controlsAttr,
                                                     true);

  Element* panel = GetElementByShadowPseudoId(MediaControls(),
                                              "-webkit-media-controls-panel");
  ASSERT_NE(nullptr, panel);

  ASSERT_TRUE(IsElementVisible(*panel));
  MediaControls().Reset();
  ASSERT_TRUE(IsElementVisible(*panel));
}

TEST_F(MediaControlsImplTest, HideAndReset) {
  MediaControls().MediaElement().SetBooleanAttribute(HTMLNames::controlsAttr,
                                                     true);

  Element* panel = GetElementByShadowPseudoId(MediaControls(),
                                              "-webkit-media-controls-panel");
  ASSERT_NE(nullptr, panel);

  ASSERT_TRUE(IsElementVisible(*panel));
  MediaControls().Hide();
  ASSERT_FALSE(IsElementVisible(*panel));
  MediaControls().Reset();
  ASSERT_FALSE(IsElementVisible(*panel));
}

TEST_F(MediaControlsImplTest, ResetDoesNotTriggerInitialLayout) {
  Document& document = this->GetDocument();
  int old_element_count = document.GetStyleEngine().StyleForElementCount();
  // Also assert that there are no layouts yet.
  ASSERT_EQ(0, old_element_count);
  MediaControls().Reset();
  int new_element_count = document.GetStyleEngine().StyleForElementCount();
  ASSERT_EQ(old_element_count, new_element_count);
}

TEST_F(MediaControlsImplTest, CastButtonRequiresRoute) {
  EnsureSizing();
  MediaControls().MediaElement().SetBooleanAttribute(HTMLNames::controlsAttr,
                                                     true);

  Element* cast_button = GetElementByShadowPseudoId(
      MediaControls(), "-internal-media-controls-cast-button");
  ASSERT_NE(nullptr, cast_button);

  ASSERT_FALSE(IsElementVisible(*cast_button));

  SimulateRouteAvailable();
  ASSERT_TRUE(IsElementVisible(*cast_button));
}

TEST_F(MediaControlsImplTest, CastButtonDisableRemotePlaybackAttr) {
  EnsureSizing();
  MediaControls().MediaElement().SetBooleanAttribute(HTMLNames::controlsAttr,
                                                     true);

  Element* cast_button = GetElementByShadowPseudoId(
      MediaControls(), "-internal-media-controls-cast-button");
  ASSERT_NE(nullptr, cast_button);

  ASSERT_FALSE(IsElementVisible(*cast_button));
  SimulateRouteAvailable();
  ASSERT_TRUE(IsElementVisible(*cast_button));

  MediaControls().MediaElement().SetBooleanAttribute(
      HTMLNames::disableremoteplaybackAttr, true);
  testing::RunPendingTasks();
  ASSERT_FALSE(IsElementVisible(*cast_button));

  MediaControls().MediaElement().SetBooleanAttribute(
      HTMLNames::disableremoteplaybackAttr, false);
  testing::RunPendingTasks();
  ASSERT_TRUE(IsElementVisible(*cast_button));
}

TEST_F(MediaControlsImplTest, CastOverlayDefault) {
  Element* cast_overlay_button = GetElementByShadowPseudoId(
      MediaControls(), "-internal-media-controls-overlay-cast-button");
  ASSERT_NE(nullptr, cast_overlay_button);

  SimulateRouteAvailable();
  ASSERT_TRUE(IsElementVisible(*cast_overlay_button));
}

TEST_F(MediaControlsImplTest, CastOverlayDisabled) {
  RuntimeEnabledFeatures::SetMediaCastOverlayButtonEnabled(false);

  Element* cast_overlay_button = GetElementByShadowPseudoId(
      MediaControls(), "-internal-media-controls-overlay-cast-button");
  ASSERT_NE(nullptr, cast_overlay_button);

  SimulateRouteAvailable();
  ASSERT_FALSE(IsElementVisible(*cast_overlay_button));
}

TEST_F(MediaControlsImplTest, CastOverlayDisableRemotePlaybackAttr) {
  Element* cast_overlay_button = GetElementByShadowPseudoId(
      MediaControls(), "-internal-media-controls-overlay-cast-button");
  ASSERT_NE(nullptr, cast_overlay_button);

  ASSERT_FALSE(IsElementVisible(*cast_overlay_button));
  SimulateRouteAvailable();
  ASSERT_TRUE(IsElementVisible(*cast_overlay_button));

  MediaControls().MediaElement().SetBooleanAttribute(
      HTMLNames::disableremoteplaybackAttr, true);
  testing::RunPendingTasks();
  ASSERT_FALSE(IsElementVisible(*cast_overlay_button));

  MediaControls().MediaElement().SetBooleanAttribute(
      HTMLNames::disableremoteplaybackAttr, false);
  testing::RunPendingTasks();
  ASSERT_TRUE(IsElementVisible(*cast_overlay_button));
}

TEST_F(MediaControlsImplTest, CastOverlayMediaControlsDisabled) {
  Element* cast_overlay_button = GetElementByShadowPseudoId(
      MediaControls(), "-internal-media-controls-overlay-cast-button");
  ASSERT_NE(nullptr, cast_overlay_button);

  EXPECT_FALSE(IsElementVisible(*cast_overlay_button));
  SimulateRouteAvailable();
  EXPECT_TRUE(IsElementVisible(*cast_overlay_button));

  GetDocument().GetSettings()->SetMediaControlsEnabled(false);
  EXPECT_FALSE(IsElementVisible(*cast_overlay_button));

  GetDocument().GetSettings()->SetMediaControlsEnabled(true);
  EXPECT_TRUE(IsElementVisible(*cast_overlay_button));
}

TEST_F(MediaControlsImplTest, CastOverlayDisabledMediaControlsDisabled) {
  RuntimeEnabledFeatures::SetMediaCastOverlayButtonEnabled(false);

  Element* cast_overlay_button = GetElementByShadowPseudoId(
      MediaControls(), "-internal-media-controls-overlay-cast-button");
  ASSERT_NE(nullptr, cast_overlay_button);

  EXPECT_FALSE(IsElementVisible(*cast_overlay_button));
  SimulateRouteAvailable();
  EXPECT_FALSE(IsElementVisible(*cast_overlay_button));

  GetDocument().GetSettings()->SetMediaControlsEnabled(false);
  EXPECT_FALSE(IsElementVisible(*cast_overlay_button));

  GetDocument().GetSettings()->SetMediaControlsEnabled(true);
  EXPECT_FALSE(IsElementVisible(*cast_overlay_button));
}

TEST_F(MediaControlsImplTest, KeepControlsVisibleIfOverflowListVisible) {
  Element* overflow_list = GetElementByShadowPseudoId(
      MediaControls(), "-internal-media-controls-overflow-menu-list");
  ASSERT_NE(nullptr, overflow_list);

  Element* panel = GetElementByShadowPseudoId(MediaControls(),
                                              "-webkit-media-controls-panel");
  ASSERT_NE(nullptr, panel);

  MediaControls().MediaElement().SetSrc("http://example.com");
  MediaControls().MediaElement().Play();
  testing::RunPendingTasks();

  MediaControls().MaybeShow();
  MediaControls().ToggleOverflowMenu();
  EXPECT_TRUE(IsElementVisible(*overflow_list));

  SimulateHideMediaControlsTimerFired();
  EXPECT_TRUE(IsElementVisible(*overflow_list));
  EXPECT_TRUE(IsElementVisible(*panel));
}

TEST_F(MediaControlsImplTest, DownloadButtonDisplayed) {
  EnsureSizing();

  Element* download_button = GetElementByShadowPseudoId(
      MediaControls(), "-internal-media-controls-download-button");
  ASSERT_NE(nullptr, download_button);

  MediaControls().MediaElement().SetSrc("https://example.com/foo.mp4");
  testing::RunPendingTasks();
  SimulateLoadedMetadata();

  // Download button should normally be displayed.
  EXPECT_TRUE(IsElementVisible(*download_button));
}

TEST_F(MediaControlsImplTest, DownloadButtonNotDisplayedEmptyUrl) {
  EnsureSizing();

  Element* download_button = GetElementByShadowPseudoId(
      MediaControls(), "-internal-media-controls-download-button");
  ASSERT_NE(nullptr, download_button);

  // Download button should not be displayed when URL is empty.
  MediaControls().MediaElement().SetSrc("");
  testing::RunPendingTasks();
  SimulateLoadedMetadata();
  EXPECT_FALSE(IsElementVisible(*download_button));
}

TEST_F(MediaControlsImplTest, DownloadButtonNotDisplayedInfiniteDuration) {
  EnsureSizing();

  Element* download_button = GetElementByShadowPseudoId(
      MediaControls(), "-internal-media-controls-download-button");
  ASSERT_NE(nullptr, download_button);

  MediaControls().MediaElement().SetSrc("https://example.com/foo.mp4");
  testing::RunPendingTasks();

  // Download button should not be displayed when duration is infinite.
  MediaControls().MediaElement().DurationChanged(
      std::numeric_limits<double>::infinity(), false /* requestSeek */);
  SimulateLoadedMetadata();
  EXPECT_FALSE(IsElementVisible(*download_button));
}

TEST_F(MediaControlsImplTest, DownloadButtonNotDisplayedHLS) {
  EnsureSizing();

  Element* download_button = GetElementByShadowPseudoId(
      MediaControls(), "-internal-media-controls-download-button");
  ASSERT_NE(nullptr, download_button);

  // Download button should not be displayed for HLS streams.
  MediaControls().MediaElement().SetSrc("https://example.com/foo.m3u8");
  testing::RunPendingTasks();
  SimulateLoadedMetadata();
  EXPECT_FALSE(IsElementVisible(*download_button));
}

TEST_F(MediaControlsImplTest, TimelineSeekToRoundedEnd) {
  EnsureSizing();

  // Tests the case where the real length of the video, |exact_duration|, gets
  // rounded up slightly to |rounded_up_duration| when setting the timeline's
  // |max| attribute (crbug.com/695065).
  double exact_duration = 596.586667;
  double rounded_up_duration = 596.587;
  LoadMediaWithDuration(exact_duration);

  // Simulate a click slightly past the end of the track of the timeline's
  // underlying <input type="range">. This would set the |value| to the |max|
  // attribute, which can be slightly rounded relative to the duration.
  MediaControlTimelineElement* timeline = TimelineElement();
  timeline->setValueAsNumber(rounded_up_duration, ASSERT_NO_EXCEPTION);
  ASSERT_EQ(rounded_up_duration, timeline->valueAsNumber());
  EXPECT_EQ(0.0, MediaControls().MediaElement().currentTime());
  timeline->DispatchInputEvent();
  EXPECT_EQ(exact_duration, MediaControls().MediaElement().currentTime());
}

TEST_F(MediaControlsImplTest, TimelineImmediatelyUpdatesCurrentTime) {
  EnsureSizing();

  MediaControlCurrentTimeDisplayElement* current_time_display =
      GetCurrentTimeDisplayElement();
  double duration = 600;
  LoadMediaWithDuration(duration);

  // Simulate seeking the underlying range to 50%. Current time display should
  // update synchronously (rather than waiting for media to finish seeking).
  TimelineElement()->setValueAsNumber(duration / 2, ASSERT_NO_EXCEPTION);
  TimelineElement()->DispatchInputEvent();
  EXPECT_EQ(duration / 2, current_time_display->CurrentValue());
}

TEST_F(MediaControlsImplTest, VolumeSliderPaintInvalidationOnInput) {
  EnsureSizing();

  Element* volume_slider = VolumeSliderElement();

  MockLayoutObject layout_object(volume_slider);
  LayoutObject* prev_layout_object = volume_slider->GetLayoutObject();
  volume_slider->SetLayoutObject(&layout_object);

  layout_object.ClearPaintInvalidationFlags();
  EXPECT_FALSE(layout_object.ShouldDoFullPaintInvalidation());
  Event* event = Event::Create(EventTypeNames::input);
  volume_slider->DefaultEventHandler(event);
  EXPECT_TRUE(layout_object.ShouldDoFullPaintInvalidation());

  layout_object.ClearPaintInvalidationFlags();
  EXPECT_FALSE(layout_object.ShouldDoFullPaintInvalidation());
  event = Event::Create(EventTypeNames::input);
  volume_slider->DefaultEventHandler(event);
  EXPECT_TRUE(layout_object.ShouldDoFullPaintInvalidation());

  layout_object.ClearPaintInvalidationFlags();
  EXPECT_FALSE(layout_object.ShouldDoFullPaintInvalidation());
  event = Event::Create(EventTypeNames::input);
  volume_slider->DefaultEventHandler(event);
  EXPECT_TRUE(layout_object.ShouldDoFullPaintInvalidation());

  volume_slider->SetLayoutObject(prev_layout_object);
}

TEST_F(MediaControlsImplTest, TimelineMetricsWidth) {
  MediaControls().MediaElement().SetSrc("https://example.com/foo.mp4");
  testing::RunPendingTasks();
  SetReady();
  EnsureSizing();
  testing::RunPendingTasks();

  MediaControlTimelineElement* timeline = TimelineElement();
  ASSERT_TRUE(IsElementVisible(*timeline));
  ASSERT_LT(0, timeline->getBoundingClientRect()->width());

  MediaControls().MediaElement().Play();
  testing::RunPendingTasks();

  GetHistogramTester().ExpectUniqueSample(
      "Media.Timeline.Width.InlineLandscape",
      timeline->getBoundingClientRect()->width(), 1);
  GetHistogramTester().ExpectTotalCount("Media.Timeline.Width.InlinePortrait",
                                        0);
  GetHistogramTester().ExpectTotalCount(
      "Media.Timeline.Width.FullscreenLandscape", 0);
  GetHistogramTester().ExpectTotalCount(
      "Media.Timeline.Width.FullscreenPortrait", 0);
}

TEST_F(MediaControlsImplTest, TimelineMetricsClick) {
  double duration = 540;  // 9 minutes
  LoadMediaWithDuration(duration);
  EnsureSizing();
  testing::RunPendingTasks();

  ASSERT_TRUE(IsElementVisible(*TimelineElement()));
  DOMRect* timelineRect = TimelineElement()->getBoundingClientRect();
  ASSERT_LT(0, timelineRect->width());

  EXPECT_EQ(0, MediaControls().MediaElement().currentTime());

  WebFloatPoint trackCenter(timelineRect->left() + timelineRect->width() / 2,
                            timelineRect->top() + timelineRect->height() / 2);
  MouseDownAt(trackCenter);
  MouseUpAt(trackCenter);
  testing::RunPendingTasks();

  EXPECT_LE(0.49 * duration, MediaControls().MediaElement().currentTime());
  EXPECT_GE(0.51 * duration, MediaControls().MediaElement().currentTime());

  GetHistogramTester().ExpectUniqueSample("Media.Timeline.SeekType." TIMELINE_W,
                                          0 /* SeekType::kClick */, 1);
  GetHistogramTester().ExpectTotalCount(
      "Media.Timeline.DragGestureDuration." TIMELINE_W, 0);
  GetHistogramTester().ExpectTotalCount(
      "Media.Timeline.DragPercent." TIMELINE_W, 0);
  GetHistogramTester().ExpectTotalCount(
      "Media.Timeline.DragSumAbsTimeDelta." TIMELINE_W, 0);
  GetHistogramTester().ExpectTotalCount(
      "Media.Timeline.DragTimeDelta." TIMELINE_W, 0);
}

TEST_F(MediaControlsImplTest, TimelineMetricsDragFromCurrentPosition) {
  double duration = 540;  // 9 minutes
  LoadMediaWithDuration(duration);
  EnsureSizing();
  testing::RunPendingTasks();

  ASSERT_TRUE(IsElementVisible(*TimelineElement()));
  DOMRect* timeline_rect = TimelineElement()->getBoundingClientRect();
  ASSERT_LT(0, timeline_rect->width());

  EXPECT_EQ(0, MediaControls().MediaElement().currentTime());

  float y = timeline_rect->top() + timeline_rect->height() / 2;
  WebFloatPoint thumb(timeline_rect->left(), y);
  WebFloatPoint track_two_thirds(
      timeline_rect->left() + timeline_rect->width() * 2 / 3, y);
  MouseDownAt(thumb);
  MouseMoveTo(track_two_thirds);
  MouseUpAt(track_two_thirds);

  EXPECT_LE(0.66 * duration, MediaControls().MediaElement().currentTime());
  EXPECT_GE(0.68 * duration, MediaControls().MediaElement().currentTime());

  GetHistogramTester().ExpectUniqueSample(
      "Media.Timeline.SeekType." TIMELINE_W,
      1 /* SeekType::kDragFromCurrentPosition */, 1);
  GetHistogramTester().ExpectTotalCount(
      "Media.Timeline.DragGestureDuration." TIMELINE_W, 1);
  GetHistogramTester().ExpectUniqueSample(
      "Media.Timeline.DragPercent." TIMELINE_W, 47 /* [60.0%, 70.0%) */, 1);
  GetHistogramTester().ExpectUniqueSample(
      "Media.Timeline.DragSumAbsTimeDelta." TIMELINE_W, 16 /* [4m, 8m) */, 1);
  GetHistogramTester().ExpectUniqueSample(
      "Media.Timeline.DragTimeDelta." TIMELINE_W, 40 /* [4m, 8m) */, 1);
}

TEST_F(MediaControlsImplTest, TimelineMetricsDragFromElsewhere) {
  double duration = 540;  // 9 minutes
  LoadMediaWithDuration(duration);
  EnsureSizing();
  testing::RunPendingTasks();

  ASSERT_TRUE(IsElementVisible(*TimelineElement()));
  DOMRect* timelineRect = TimelineElement()->getBoundingClientRect();
  ASSERT_LT(0, timelineRect->width());

  EXPECT_EQ(0, MediaControls().MediaElement().currentTime());

  float y = timelineRect->top() + timelineRect->height() / 2;
  WebFloatPoint trackOneThird(
      timelineRect->left() + timelineRect->width() * 1 / 3, y);
  WebFloatPoint trackTwoThirds(
      timelineRect->left() + timelineRect->width() * 2 / 3, y);
  MouseDownAt(trackOneThird);
  MouseMoveTo(trackTwoThirds);
  MouseUpAt(trackTwoThirds);

  EXPECT_LE(0.66 * duration, MediaControls().MediaElement().currentTime());
  EXPECT_GE(0.68 * duration, MediaControls().MediaElement().currentTime());

  GetHistogramTester().ExpectUniqueSample("Media.Timeline.SeekType." TIMELINE_W,
                                          2 /* SeekType::kDragFromElsewhere */,
                                          1);
  GetHistogramTester().ExpectTotalCount(
      "Media.Timeline.DragGestureDuration." TIMELINE_W, 1);
  GetHistogramTester().ExpectUniqueSample(
      "Media.Timeline.DragPercent." TIMELINE_W, 42 /* [30.0%, 35.0%) */, 1);
  GetHistogramTester().ExpectUniqueSample(
      "Media.Timeline.DragSumAbsTimeDelta." TIMELINE_W, 15 /* [2m, 4m) */, 1);
  GetHistogramTester().ExpectUniqueSample(
      "Media.Timeline.DragTimeDelta." TIMELINE_W, 39 /* [2m, 4m) */, 1);
}

TEST_F(MediaControlsImplTest, TimelineMetricsDragBackAndForth) {
  double duration = 540;  // 9 minutes
  LoadMediaWithDuration(duration);
  EnsureSizing();
  testing::RunPendingTasks();

  ASSERT_TRUE(IsElementVisible(*TimelineElement()));
  DOMRect* timelineRect = TimelineElement()->getBoundingClientRect();
  ASSERT_LT(0, timelineRect->width());

  EXPECT_EQ(0, MediaControls().MediaElement().currentTime());

  float y = timelineRect->top() + timelineRect->height() / 2;
  WebFloatPoint trackTwoThirds(
      timelineRect->left() + timelineRect->width() * 2 / 3, y);
  WebFloatPoint trackEnd(timelineRect->left() + timelineRect->width(), y);
  WebFloatPoint trackOneThird(
      timelineRect->left() + timelineRect->width() * 1 / 3, y);
  MouseDownAt(trackTwoThirds);
  MouseMoveTo(trackEnd);
  MouseMoveTo(trackOneThird);
  MouseUpAt(trackOneThird);

  EXPECT_LE(0.32 * duration, MediaControls().MediaElement().currentTime());
  EXPECT_GE(0.34 * duration, MediaControls().MediaElement().currentTime());

  GetHistogramTester().ExpectUniqueSample("Media.Timeline.SeekType." TIMELINE_W,
                                          2 /* SeekType::kDragFromElsewhere */,
                                          1);
  GetHistogramTester().ExpectTotalCount(
      "Media.Timeline.DragGestureDuration." TIMELINE_W, 1);
  GetHistogramTester().ExpectUniqueSample(
      "Media.Timeline.DragPercent." TIMELINE_W, 8 /* (-35.0%, -30.0%] */, 1);
  GetHistogramTester().ExpectUniqueSample(
      "Media.Timeline.DragSumAbsTimeDelta." TIMELINE_W, 17 /* [8m, 15m) */, 1);
  GetHistogramTester().ExpectUniqueSample(
      "Media.Timeline.DragTimeDelta." TIMELINE_W, 9 /* (-4m, -2m] */, 1);
}

namespace {

class MediaControlsImplTestWithMockScheduler : public MediaControlsImplTest {
 protected:
  void SetUp() override {
    // DocumentParserTiming has DCHECKS to make sure time > 0.0.
    platform_->AdvanceClockSeconds(1);

    MediaControlsImplTest::SetUp();
  }

  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform_;
};

}  // namespace

TEST_F(MediaControlsImplTestWithMockScheduler,
       ControlsRemainVisibleDuringKeyboardInteraction) {
  EnsureSizing();

  Element* panel = MediaControls().PanelElement();

  MediaControls().MediaElement().SetBooleanAttribute(HTMLNames::controlsAttr,
                                                     true);
  MediaControls().MediaElement().SetSrc("http://example.com");
  MediaControls().MediaElement().Play();

  // Controls start out visible.
  EXPECT_TRUE(IsElementVisible(*panel));

  // Tabbing between controls prevents controls from hiding.
  platform_->RunForPeriodSeconds(2);
  MediaControls().DispatchEvent(Event::Create("focusin"));
  platform_->RunForPeriodSeconds(2);
  EXPECT_TRUE(IsElementVisible(*panel));

  // Seeking on the timeline or volume bar prevents controls from hiding.
  MediaControls().DispatchEvent(Event::Create("input"));
  platform_->RunForPeriodSeconds(2);
  EXPECT_TRUE(IsElementVisible(*panel));

  // Pressing a key prevents controls from hiding.
  MediaControls().PanelElement()->DispatchEvent(Event::Create("keypress"));
  platform_->RunForPeriodSeconds(2);
  EXPECT_TRUE(IsElementVisible(*panel));

  // Once user interaction stops, controls can hide.
  platform_->RunForPeriodSeconds(2);
  EXPECT_FALSE(IsElementVisible(*panel));
}

TEST_F(MediaControlsImplTest,
       RemovingFromDocumentRemovesListenersAndCallbacks) {
  auto page_holder = DummyPageHolder::Create();

  HTMLMediaElement* element =
      HTMLVideoElement::Create(page_holder->GetDocument());
  element->SetBooleanAttribute(HTMLNames::controlsAttr, true);
  page_holder->GetDocument().body()->AppendChild(element);

  RemotePlayback* remote_playback =
      HTMLMediaElementRemotePlayback::remote(*element);

  EXPECT_TRUE(remote_playback->HasEventListeners());
  EXPECT_TRUE(HasAvailabilityCallbacks(remote_playback));

  WeakPersistent<HTMLMediaElement> weak_persistent_video = element;
  {
    Persistent<HTMLMediaElement> persistent_video = element;
    page_holder->GetDocument().body()->setInnerHTML("");

    // When removed from the document, the event listeners should have been
    // dropped.
    EXPECT_FALSE(remote_playback->HasEventListeners());
    EXPECT_FALSE(HasAvailabilityCallbacks(remote_playback));
  }

  testing::RunPendingTasks();

  ThreadState::Current()->CollectAllGarbage();

  // It has been GC'd.
  EXPECT_EQ(nullptr, weak_persistent_video);
}

TEST_F(MediaControlsImplTest,
       ReInsertingInDocumentRestoresListenersAndCallbacks) {
  auto page_holder = DummyPageHolder::Create();

  HTMLMediaElement* element =
      HTMLVideoElement::Create(page_holder->GetDocument());
  element->SetBooleanAttribute(HTMLNames::controlsAttr, true);
  page_holder->GetDocument().body()->AppendChild(element);

  RemotePlayback* remote_playback =
      HTMLMediaElementRemotePlayback::remote(*element);

  // This should be a no-op. We keep a reference on the media element to avoid
  // an unexpected GC.
  {
    Persistent<HTMLMediaElement> video_holder = element;
    page_holder->GetDocument().body()->RemoveChild(element);
    page_holder->GetDocument().body()->AppendChild(video_holder.Get());
    EXPECT_TRUE(remote_playback->HasEventListeners());
    EXPECT_TRUE(HasAvailabilityCallbacks(remote_playback));
  }
}

}  // namespace blink

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/media_controls/MediaControlsImpl.h"

#include <limits>
#include <memory>
#include "core/HTMLNames.h"
#include "core/css/StylePropertySet.h"
#include "core/dom/ClientRect.h"
#include "core/dom/Document.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/StyleEngine.h"
#include "core/events/Event.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLElement.h"
#include "core/html/HTMLVideoElement.h"
#include "core/html/shadow/MediaControlElementTypes.h"
#include "core/input/EventHandler.h"
#include "core/layout/LayoutObject.h"
#include "core/loader/EmptyClients.h"
#include "core/testing/DummyPageHolder.h"
#include "modules/media_controls/elements/MediaControlCurrentTimeDisplayElement.h"
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

namespace blink {

namespace {

class MockChromeClient : public EmptyChromeClient {
 public:
  // EmptyChromeClient overrides:
  WebScreenInfo GetScreenInfo() const override {
    WebScreenInfo screen_info;
    screen_info.orientation_type = kWebScreenOrientationLandscapePrimary;
    return screen_info;
  }
};

class MockVideoWebMediaPlayer : public EmptyWebMediaPlayer {
 public:
  // WebMediaPlayer overrides:
  WebTimeRanges Seekable() const override { return seekable_; }
  bool HasVideo() const override { return true; }

  WebTimeRanges seekable_;
};

class MockWebRemotePlaybackClient : public WebRemotePlaybackClient {
 public:
  void StateChanged(WebRemotePlaybackState) override {}
  void AvailabilityChanged(
      WebRemotePlaybackAvailability availability) override {
    availability_ = availability;
  }
  void PromptCancelled() override {}
  bool RemotePlaybackAvailable() const override {
    return availability_ == WebRemotePlaybackAvailability::kDeviceAvailable;
  }

 private:
  WebRemotePlaybackAvailability availability_ =
      WebRemotePlaybackAvailability::kUnknown;
};

class MockLayoutObject : public LayoutObject {
 public:
  MockLayoutObject() : LayoutObject(nullptr) {}

  const char* GetName() const override { return "MockLayoutObject"; }
  void UpdateLayout() override {}
  FloatRect LocalBoundingBoxRectForAccessibility() const override {
    return FloatRect();
  }

  void SetShouldDoFullPaintInvalidation(PaintInvalidationReason) {
    full_paint_invalidation_call_count_++;
  }

  int FullPaintInvalidationCallCount() const {
    return full_paint_invalidation_call_count_;
  }

 private:
  int full_paint_invalidation_call_count_ = 0;
};

class StubLocalFrameClient : public EmptyLocalFrameClient {
 public:
  static StubLocalFrameClient* Create() { return new StubLocalFrameClient; }

  std::unique_ptr<WebMediaPlayer> CreateWebMediaPlayer(
      HTMLMediaElement&,
      const WebMediaPlayerSource&,
      WebMediaPlayerClient*) override {
    return WTF::WrapUnique(new MockVideoWebMediaPlayer);
  }

  WebRemotePlaybackClient* CreateWebRemotePlaybackClient(
      HTMLMediaElement&) override {
    if (!remote_playback_client_) {
      remote_playback_client_ =
          WTF::WrapUnique(new MockWebRemotePlaybackClient);
    }
    return remote_playback_client_.get();
  }

 private:
  std::unique_ptr<MockWebRemotePlaybackClient> remote_playback_client_;
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
  virtual void SetUp() { InitializePage(); }

  void InitializePage() {
    Page::PageClients clients;
    FillWithEmptyClients(clients);
    clients.chrome_client = new MockChromeClient();
    page_holder_ = DummyPageHolder::Create(IntSize(800, 600), &clients,
                                           StubLocalFrameClient::Create());

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
  MockVideoWebMediaPlayer* WebMediaPlayer() {
    return static_cast<MockVideoWebMediaPlayer*>(
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
  MediaControls().Show();
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
  ASSERT_FALSE(IsElementVisible(*cast_button));

  MediaControls().MediaElement().SetBooleanAttribute(
      HTMLNames::disableremoteplaybackAttr, false);
  ASSERT_TRUE(IsElementVisible(*cast_button));
}

TEST_F(MediaControlsImplTest, CastOverlayDefault) {
  Element* cast_overlay_button = GetElementByShadowPseudoId(
      MediaControls(), "-internal-media-controls-overlay-cast-button");
  ASSERT_NE(nullptr, cast_overlay_button);

  SimulateRouteAvailable();
  ASSERT_TRUE(IsElementVisible(*cast_overlay_button));
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
  ASSERT_FALSE(IsElementVisible(*cast_overlay_button));

  MediaControls().MediaElement().SetBooleanAttribute(
      HTMLNames::disableremoteplaybackAttr, false);
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

  MediaControls().Show();
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

TEST_F(MediaControlsImplTest, DownloadButtonDisplayedHiddenAndDisplayed) {
  EnsureSizing();

  Element* download_button = GetElementByShadowPseudoId(
      MediaControls(), "-internal-media-controls-download-button");
  ASSERT_NE(nullptr, download_button);

  // Initially show button.
  MediaControls().MediaElement().SetSrc("https://example.com/foo.mp4");
  testing::RunPendingTasks();
  SimulateLoadedMetadata();
  EXPECT_TRUE(IsElementVisible(*download_button));
  GetHistogramTester().ExpectBucketCount("Media.Controls.Download",
                                         DownloadActionMetrics::kShown, 1);

  // Hide button.
  MediaControls().MediaElement().SetSrc("");
  testing::RunPendingTasks();
  EXPECT_FALSE(IsElementVisible(*download_button));
  GetHistogramTester().ExpectBucketCount("Media.Controls.Download",
                                         DownloadActionMetrics::kShown, 1);

  // Showing button again should not increment Shown count.
  MediaControls().MediaElement().SetSrc("https://example.com/foo.mp4");
  testing::RunPendingTasks();
  EXPECT_TRUE(IsElementVisible(*download_button));
  GetHistogramTester().ExpectBucketCount("Media.Controls.Download",
                                         DownloadActionMetrics::kShown, 1);
}

TEST_F(MediaControlsImplTest, DownloadButtonRecordsClickOnlyOnce) {
  EnsureSizing();

  MediaControlDownloadButtonElement* download_button =
      static_cast<MediaControlDownloadButtonElement*>(
          GetElementByShadowPseudoId(
              MediaControls(), "-internal-media-controls-download-button"));
  ASSERT_NE(nullptr, download_button);

  // Initially show button.
  MediaControls().MediaElement().SetSrc("https://example.com/foo.mp4");
  testing::RunPendingTasks();
  SimulateLoadedMetadata();
  EXPECT_TRUE(IsElementVisible(*download_button));
  GetHistogramTester().ExpectBucketCount("Media.Controls.Download",
                                         DownloadActionMetrics::kShown, 1);

  // Click button once.
  download_button->DispatchSimulatedClick(
      Event::CreateBubble(EventTypeNames::click), kSendNoEvents);
  GetHistogramTester().ExpectBucketCount("Media.Controls.Download",
                                         DownloadActionMetrics::kClicked, 1);

  // Clicking button again should not increment Clicked count.
  download_button->DispatchSimulatedClick(
      Event::CreateBubble(EventTypeNames::click), kSendNoEvents);
  GetHistogramTester().ExpectBucketCount("Media.Controls.Download",
                                         DownloadActionMetrics::kClicked, 1);
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

  MockLayoutObject layout_object;
  LayoutObject* prev_layout_object = volume_slider->GetLayoutObject();
  volume_slider->SetLayoutObject(&layout_object);

  Event* event = Event::Create(EventTypeNames::input);
  volume_slider->DefaultEventHandler(event);
  EXPECT_EQ(1, layout_object.FullPaintInvalidationCallCount());

  event = Event::Create(EventTypeNames::input);
  volume_slider->DefaultEventHandler(event);
  EXPECT_EQ(2, layout_object.FullPaintInvalidationCallCount());

  event = Event::Create(EventTypeNames::input);
  volume_slider->DefaultEventHandler(event);
  EXPECT_EQ(3, layout_object.FullPaintInvalidationCallCount());

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

// TODO(johnme): Fix and re-enable this on Android.
#if OS(ANDROID)
#define MAYBE_TimelineMetricsClick DISABLED_TimelineMetricsClick
#else
#define MAYBE_TimelineMetricsClick TimelineMetricsClick
#endif
TEST_F(MediaControlsImplTest, MAYBE_TimelineMetricsClick) {
  double duration = 540;  // 9 minutes
  LoadMediaWithDuration(duration);
  EnsureSizing();
  testing::RunPendingTasks();

  ASSERT_TRUE(IsElementVisible(*TimelineElement()));
  ClientRect* timelineRect = TimelineElement()->getBoundingClientRect();
  ASSERT_LT(0, timelineRect->width());

  EXPECT_EQ(0, MediaControls().MediaElement().currentTime());

  WebFloatPoint trackCenter(timelineRect->left() + timelineRect->width() / 2,
                            timelineRect->top() + timelineRect->height() / 2);
  MouseDownAt(trackCenter);
  MouseUpAt(trackCenter);
  testing::RunPendingTasks();

  EXPECT_LE(0.49 * duration, MediaControls().MediaElement().currentTime());
  EXPECT_GE(0.51 * duration, MediaControls().MediaElement().currentTime());

  GetHistogramTester().ExpectUniqueSample("Media.Timeline.SeekType.128_255",
                                          0 /* SeekType::kClick */, 1);
  GetHistogramTester().ExpectTotalCount(
      "Media.Timeline.DragGestureDuration.128_255", 0);
  GetHistogramTester().ExpectTotalCount("Media.Timeline.DragPercent.128_255",
                                        0);
  GetHistogramTester().ExpectTotalCount(
      "Media.Timeline.DragSumAbsTimeDelta.128_255", 0);
  GetHistogramTester().ExpectTotalCount("Media.Timeline.DragTimeDelta.128_255",
                                        0);
}

// TODO(johnme): Fix and re-enable this on Android.
#if OS(ANDROID)
#define MAYBE_TimelineMetricsDragFromCurrentPosition \
  DISABLED_TimelineMetricsDragFromCurrentPosition
#else
#define MAYBE_TimelineMetricsDragFromCurrentPosition \
  TimelineMetricsDragFromCurrentPosition
#endif
TEST_F(MediaControlsImplTest, MAYBE_TimelineMetricsDragFromCurrentPosition) {
  double duration = 540;  // 9 minutes
  LoadMediaWithDuration(duration);
  EnsureSizing();
  testing::RunPendingTasks();

  ASSERT_TRUE(IsElementVisible(*TimelineElement()));
  ClientRect* timeline_rect = TimelineElement()->getBoundingClientRect();
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
      "Media.Timeline.SeekType.128_255",
      1 /* SeekType::kDragFromCurrentPosition */, 1);
  GetHistogramTester().ExpectTotalCount(
      "Media.Timeline.DragGestureDuration.128_255", 1);
  GetHistogramTester().ExpectUniqueSample("Media.Timeline.DragPercent.128_255",
                                          47 /* [60.0%, 70.0%) */, 1);
  GetHistogramTester().ExpectUniqueSample(
      "Media.Timeline.DragSumAbsTimeDelta.128_255", 16 /* [4m, 8m) */, 1);
  GetHistogramTester().ExpectUniqueSample(
      "Media.Timeline.DragTimeDelta.128_255", 40 /* [4m, 8m) */, 1);
}

// TODO(johnme): Fix and re-enable this on Android.
#if OS(ANDROID)
#define MAYBE_TimelineMetricsDragFromElsewhere \
  DISABLED_TimelineMetricsDragFromElsewhere
#else
#define MAYBE_TimelineMetricsDragFromElsewhere TimelineMetricsDragFromElsewhere
#endif
TEST_F(MediaControlsImplTest, MAYBE_TimelineMetricsDragFromElsewhere) {
  double duration = 540;  // 9 minutes
  LoadMediaWithDuration(duration);
  EnsureSizing();
  testing::RunPendingTasks();

  ASSERT_TRUE(IsElementVisible(*TimelineElement()));
  ClientRect* timelineRect = TimelineElement()->getBoundingClientRect();
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

  GetHistogramTester().ExpectUniqueSample("Media.Timeline.SeekType.128_255",
                                          2 /* SeekType::kDragFromElsewhere */,
                                          1);
  GetHistogramTester().ExpectTotalCount(
      "Media.Timeline.DragGestureDuration.128_255", 1);
  GetHistogramTester().ExpectUniqueSample("Media.Timeline.DragPercent.128_255",
                                          42 /* [30.0%, 35.0%) */, 1);
  GetHistogramTester().ExpectUniqueSample(
      "Media.Timeline.DragSumAbsTimeDelta.128_255", 15 /* [2m, 4m) */, 1);
  GetHistogramTester().ExpectUniqueSample(
      "Media.Timeline.DragTimeDelta.128_255", 39 /* [2m, 4m) */, 1);
}

// TODO(johnme): Fix and re-enable this on Android.
#if OS(ANDROID)
#define MAYBE_TimelineMetricsDragBackAndForth \
  DISABLED_TimelineMetricsDragBackAndForth
#else
#define MAYBE_TimelineMetricsDragBackAndForth TimelineMetricsDragBackAndForth
#endif
TEST_F(MediaControlsImplTest, MAYBE_TimelineMetricsDragBackAndForth) {
  double duration = 540;  // 9 minutes
  LoadMediaWithDuration(duration);
  EnsureSizing();
  testing::RunPendingTasks();

  ASSERT_TRUE(IsElementVisible(*TimelineElement()));
  ClientRect* timelineRect = TimelineElement()->getBoundingClientRect();
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

  GetHistogramTester().ExpectUniqueSample("Media.Timeline.SeekType.128_255",
                                          2 /* SeekType::kDragFromElsewhere */,
                                          1);
  GetHistogramTester().ExpectTotalCount(
      "Media.Timeline.DragGestureDuration.128_255", 1);
  GetHistogramTester().ExpectUniqueSample("Media.Timeline.DragPercent.128_255",
                                          8 /* (-35.0%, -30.0%] */, 1);
  GetHistogramTester().ExpectUniqueSample(
      "Media.Timeline.DragSumAbsTimeDelta.128_255", 17 /* [8m, 15m) */, 1);
  GetHistogramTester().ExpectUniqueSample(
      "Media.Timeline.DragTimeDelta.128_255", 9 /* (-4m, -2m] */, 1);
}

TEST_F(MediaControlsImplTest, ControlsRemainVisibleDuringKeyboardInteraction) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform;

  // DocumentParserTiming has DCHECKS to make sure time > 0.0.
  platform->AdvanceClockSeconds(1);

  // Need to reinitialize page since we changed the platform.
  InitializePage();
  EnsureSizing();

  Element* panel = MediaControls().PanelElement();

  MediaControls().MediaElement().SetBooleanAttribute(HTMLNames::controlsAttr,
                                                     true);
  MediaControls().MediaElement().SetSrc("http://example.com");
  MediaControls().MediaElement().Play();

  // Controls start out visible.
  EXPECT_TRUE(IsElementVisible(*panel));

  // Tabbing between controls prevents controls from hiding.
  platform->RunForPeriodSeconds(2);
  MediaControls().DispatchEvent(Event::Create("focusin"));
  platform->RunForPeriodSeconds(2);
  EXPECT_TRUE(IsElementVisible(*panel));

  // Seeking on the timeline or volume bar prevents controls from hiding.
  MediaControls().DispatchEvent(Event::Create("input"));
  platform->RunForPeriodSeconds(2);
  EXPECT_TRUE(IsElementVisible(*panel));

  // Pressing a key prevents controls from hiding.
  MediaControls().PanelElement()->DispatchEvent(Event::Create("keypress"));
  platform->RunForPeriodSeconds(2);
  EXPECT_TRUE(IsElementVisible(*panel));

  // Once user interaction stops, controls can hide.
  platform->RunForPeriodSeconds(2);
  EXPECT_FALSE(IsElementVisible(*panel));
}

}  // namespace blink

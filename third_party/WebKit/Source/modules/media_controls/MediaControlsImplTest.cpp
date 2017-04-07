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
#include "platform/heap/Handle.h"
#include "platform/testing/EmptyWebMediaPlayer.h"
#include "platform/testing/HistogramTester.h"
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
  WebScreenInfo screenInfo() const override {
    WebScreenInfo screenInfo;
    screenInfo.orientationType = WebScreenOrientationLandscapePrimary;
    return screenInfo;
  }
};

class MockVideoWebMediaPlayer : public EmptyWebMediaPlayer {
 public:
  // WebMediaPlayer overrides:
  WebTimeRanges seekable() const override { return m_seekable; }
  bool hasVideo() const override { return true; }

  WebTimeRanges m_seekable;
};

class MockWebRemotePlaybackClient : public WebRemotePlaybackClient {
 public:
  void stateChanged(WebRemotePlaybackState) override {}
  void availabilityChanged(
      WebRemotePlaybackAvailability availability) override {
    m_availability = availability;
  }
  void promptCancelled() override {}
  bool remotePlaybackAvailable() const override {
    return m_availability == WebRemotePlaybackAvailability::DeviceAvailable;
  }

 private:
  WebRemotePlaybackAvailability m_availability =
      WebRemotePlaybackAvailability::Unknown;
};

class MockLayoutObject : public LayoutObject {
 public:
  MockLayoutObject() : LayoutObject(nullptr) {}

  const char* name() const override { return "MockLayoutObject"; }
  void layout() override {}
  FloatRect localBoundingBoxRectForAccessibility() const override {
    return FloatRect();
  }

  void setShouldDoFullPaintInvalidation(PaintInvalidationReason) {
    m_fullPaintInvalidationCallCount++;
  }

  int fullPaintInvalidationCallCount() const {
    return m_fullPaintInvalidationCallCount;
  }

 private:
  int m_fullPaintInvalidationCallCount = 0;
};

class StubLocalFrameClient : public EmptyLocalFrameClient {
 public:
  static StubLocalFrameClient* create() { return new StubLocalFrameClient; }

  std::unique_ptr<WebMediaPlayer> createWebMediaPlayer(
      HTMLMediaElement&,
      const WebMediaPlayerSource&,
      WebMediaPlayerClient*) override {
    return WTF::wrapUnique(new MockVideoWebMediaPlayer);
  }

  WebRemotePlaybackClient* createWebRemotePlaybackClient(
      HTMLMediaElement&) override {
    if (!m_remotePlaybackClient) {
      m_remotePlaybackClient = WTF::wrapUnique(new MockWebRemotePlaybackClient);
    }
    return m_remotePlaybackClient.get();
  }

 private:
  std::unique_ptr<MockWebRemotePlaybackClient> m_remotePlaybackClient;
};

Element* getElementByShadowPseudoId(Node& rootNode,
                                    const char* shadowPseudoId) {
  for (Element& element : ElementTraversal::descendantsOf(rootNode)) {
    if (element.shadowPseudoId() == shadowPseudoId)
      return &element;
  }
  return nullptr;
}

bool isElementVisible(Element& element) {
  const StylePropertySet* inlineStyle = element.inlineStyle();

  if (!inlineStyle)
    return true;

  if (inlineStyle->getPropertyValue(CSSPropertyDisplay) == "none")
    return false;

  if (inlineStyle->hasProperty(CSSPropertyOpacity) &&
      inlineStyle->getPropertyValue(CSSPropertyOpacity).toDouble() == 0.0) {
    return false;
  }

  if (inlineStyle->getPropertyValue(CSSPropertyVisibility) == "hidden")
    return false;

  if (Element* parent = element.parentElement())
    return isElementVisible(*parent);

  return true;
}

// This must match MediaControlDownloadButtonElement::DownloadActionMetrics.
enum DownloadActionMetrics {
  Shown = 0,
  Clicked,
  Count  // Keep last.
};

}  // namespace

class MediaControlsImplTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    Page::PageClients clients;
    fillWithEmptyClients(clients);
    clients.chromeClient = new MockChromeClient();
    m_pageHolder = DummyPageHolder::create(IntSize(800, 600), &clients,
                                           StubLocalFrameClient::create());

    document().write("<video>");
    HTMLVideoElement& video =
        toHTMLVideoElement(*document().querySelector("video"));
    m_mediaControls = static_cast<MediaControlsImpl*>(video.mediaControls());

    // If scripts are not enabled, controls will always be shown.
    m_pageHolder->frame().settings()->setScriptEnabled(true);
  }

  void simulateRouteAvailabe() {
    m_mediaControls->mediaElement().remoteRouteAvailabilityChanged(
        WebRemotePlaybackAvailability::DeviceAvailable);
  }

  void ensureSizing() {
    // Fire the size-change callback to ensure that the controls have
    // been properly notified of the video size.
    m_mediaControls->notifyElementSizeChanged(
        m_mediaControls->mediaElement().getBoundingClientRect());
  }

  void simulateHideMediaControlsTimerFired() {
    m_mediaControls->hideMediaControlsTimerFired(nullptr);
  }

  void simulateLoadedMetadata() { m_mediaControls->onLoadedMetadata(); }

  MediaControlsImpl& mediaControls() { return *m_mediaControls; }
  MockVideoWebMediaPlayer* webMediaPlayer() {
    return static_cast<MockVideoWebMediaPlayer*>(
        mediaControls().mediaElement().webMediaPlayer());
  }
  Document& document() { return m_pageHolder->document(); }

  HistogramTester& histogramTester() { return m_histogramTester; }

  void loadMediaWithDuration(double duration) {
    mediaControls().mediaElement().setSrc("https://example.com/foo.mp4");
    testing::runPendingTasks();
    WebTimeRange timeRange(0.0, duration);
    webMediaPlayer()->m_seekable.assign(&timeRange, 1);
    mediaControls().mediaElement().durationChanged(duration,
                                                   false /* requestSeek */);
    simulateLoadedMetadata();
  }

  void setReady() {
    mediaControls().mediaElement().setReadyState(
        HTMLMediaElement::kHaveEnoughData);
  }

  void mouseDownAt(WebFloatPoint pos);
  void mouseMoveTo(WebFloatPoint pos);
  void mouseUpAt(WebFloatPoint pos);

 private:
  std::unique_ptr<DummyPageHolder> m_pageHolder;
  Persistent<MediaControlsImpl> m_mediaControls;
  HistogramTester m_histogramTester;
};

void MediaControlsImplTest::mouseDownAt(WebFloatPoint pos) {
  WebMouseEvent mouseDownEvent(WebInputEvent::MouseDown, pos /* client pos */,
                               pos /* screen pos */,
                               WebPointerProperties::Button::Left, 1,
                               WebInputEvent::Modifiers::LeftButtonDown,
                               WebInputEvent::TimeStampForTesting);
  mouseDownEvent.setFrameScale(1);
  document().frame()->eventHandler().handleMousePressEvent(mouseDownEvent);
}

void MediaControlsImplTest::mouseMoveTo(WebFloatPoint pos) {
  WebMouseEvent mouseMoveEvent(WebInputEvent::MouseMove, pos /* client pos */,
                               pos /* screen pos */,
                               WebPointerProperties::Button::Left, 1,
                               WebInputEvent::Modifiers::LeftButtonDown,
                               WebInputEvent::TimeStampForTesting);
  mouseMoveEvent.setFrameScale(1);
  document().frame()->eventHandler().handleMouseMoveEvent(mouseMoveEvent, {});
}

void MediaControlsImplTest::mouseUpAt(WebFloatPoint pos) {
  WebMouseEvent mouseUpEvent(
      WebMouseEvent::MouseUp, pos /* client pos */, pos /* screen pos */,
      WebPointerProperties::Button::Left, 1, WebInputEvent::NoModifiers,
      WebInputEvent::TimeStampForTesting);
  mouseUpEvent.setFrameScale(1);
  document().frame()->eventHandler().handleMouseReleaseEvent(mouseUpEvent);
}

TEST_F(MediaControlsImplTest, HideAndShow) {
  mediaControls().mediaElement().setBooleanAttribute(HTMLNames::controlsAttr,
                                                     true);

  Element* panel = getElementByShadowPseudoId(mediaControls(),
                                              "-webkit-media-controls-panel");
  ASSERT_NE(nullptr, panel);

  ASSERT_TRUE(isElementVisible(*panel));
  mediaControls().hide();
  ASSERT_FALSE(isElementVisible(*panel));
  mediaControls().show();
  ASSERT_TRUE(isElementVisible(*panel));
}

TEST_F(MediaControlsImplTest, Reset) {
  mediaControls().mediaElement().setBooleanAttribute(HTMLNames::controlsAttr,
                                                     true);

  Element* panel = getElementByShadowPseudoId(mediaControls(),
                                              "-webkit-media-controls-panel");
  ASSERT_NE(nullptr, panel);

  ASSERT_TRUE(isElementVisible(*panel));
  mediaControls().reset();
  ASSERT_TRUE(isElementVisible(*panel));
}

TEST_F(MediaControlsImplTest, HideAndReset) {
  mediaControls().mediaElement().setBooleanAttribute(HTMLNames::controlsAttr,
                                                     true);

  Element* panel = getElementByShadowPseudoId(mediaControls(),
                                              "-webkit-media-controls-panel");
  ASSERT_NE(nullptr, panel);

  ASSERT_TRUE(isElementVisible(*panel));
  mediaControls().hide();
  ASSERT_FALSE(isElementVisible(*panel));
  mediaControls().reset();
  ASSERT_FALSE(isElementVisible(*panel));
}

TEST_F(MediaControlsImplTest, ResetDoesNotTriggerInitialLayout) {
  Document& document = this->document();
  int oldElementCount = document.styleEngine().styleForElementCount();
  // Also assert that there are no layouts yet.
  ASSERT_EQ(0, oldElementCount);
  mediaControls().reset();
  int newElementCount = document.styleEngine().styleForElementCount();
  ASSERT_EQ(oldElementCount, newElementCount);
}

TEST_F(MediaControlsImplTest, CastButtonRequiresRoute) {
  ensureSizing();
  mediaControls().mediaElement().setBooleanAttribute(HTMLNames::controlsAttr,
                                                     true);

  Element* castButton = getElementByShadowPseudoId(
      mediaControls(), "-internal-media-controls-cast-button");
  ASSERT_NE(nullptr, castButton);

  ASSERT_FALSE(isElementVisible(*castButton));

  simulateRouteAvailabe();
  ASSERT_TRUE(isElementVisible(*castButton));
}

TEST_F(MediaControlsImplTest, CastButtonDisableRemotePlaybackAttr) {
  ensureSizing();
  mediaControls().mediaElement().setBooleanAttribute(HTMLNames::controlsAttr,
                                                     true);

  Element* castButton = getElementByShadowPseudoId(
      mediaControls(), "-internal-media-controls-cast-button");
  ASSERT_NE(nullptr, castButton);

  ASSERT_FALSE(isElementVisible(*castButton));
  simulateRouteAvailabe();
  ASSERT_TRUE(isElementVisible(*castButton));

  mediaControls().mediaElement().setBooleanAttribute(
      HTMLNames::disableremoteplaybackAttr, true);
  ASSERT_FALSE(isElementVisible(*castButton));

  mediaControls().mediaElement().setBooleanAttribute(
      HTMLNames::disableremoteplaybackAttr, false);
  ASSERT_TRUE(isElementVisible(*castButton));
}

TEST_F(MediaControlsImplTest, CastOverlayDefault) {
  Element* castOverlayButton = getElementByShadowPseudoId(
      mediaControls(), "-internal-media-controls-overlay-cast-button");
  ASSERT_NE(nullptr, castOverlayButton);

  simulateRouteAvailabe();
  ASSERT_TRUE(isElementVisible(*castOverlayButton));
}

TEST_F(MediaControlsImplTest, CastOverlayDisableRemotePlaybackAttr) {
  Element* castOverlayButton = getElementByShadowPseudoId(
      mediaControls(), "-internal-media-controls-overlay-cast-button");
  ASSERT_NE(nullptr, castOverlayButton);

  ASSERT_FALSE(isElementVisible(*castOverlayButton));
  simulateRouteAvailabe();
  ASSERT_TRUE(isElementVisible(*castOverlayButton));

  mediaControls().mediaElement().setBooleanAttribute(
      HTMLNames::disableremoteplaybackAttr, true);
  ASSERT_FALSE(isElementVisible(*castOverlayButton));

  mediaControls().mediaElement().setBooleanAttribute(
      HTMLNames::disableremoteplaybackAttr, false);
  ASSERT_TRUE(isElementVisible(*castOverlayButton));
}

TEST_F(MediaControlsImplTest, CastOverlayMediaControlsDisabled) {
  Element* castOverlayButton = getElementByShadowPseudoId(
      mediaControls(), "-internal-media-controls-overlay-cast-button");
  ASSERT_NE(nullptr, castOverlayButton);

  EXPECT_FALSE(isElementVisible(*castOverlayButton));
  simulateRouteAvailabe();
  EXPECT_TRUE(isElementVisible(*castOverlayButton));

  document().settings()->setMediaControlsEnabled(false);
  EXPECT_FALSE(isElementVisible(*castOverlayButton));

  document().settings()->setMediaControlsEnabled(true);
  EXPECT_TRUE(isElementVisible(*castOverlayButton));
}

TEST_F(MediaControlsImplTest, KeepControlsVisibleIfOverflowListVisible) {
  Element* overflowList = getElementByShadowPseudoId(
      mediaControls(), "-internal-media-controls-overflow-menu-list");
  ASSERT_NE(nullptr, overflowList);

  Element* panel = getElementByShadowPseudoId(mediaControls(),
                                              "-webkit-media-controls-panel");
  ASSERT_NE(nullptr, panel);

  mediaControls().mediaElement().setSrc("http://example.com");
  mediaControls().mediaElement().play();
  testing::runPendingTasks();

  mediaControls().show();
  mediaControls().toggleOverflowMenu();
  EXPECT_TRUE(isElementVisible(*overflowList));

  simulateHideMediaControlsTimerFired();
  EXPECT_TRUE(isElementVisible(*overflowList));
  EXPECT_TRUE(isElementVisible(*panel));
}

TEST_F(MediaControlsImplTest, DownloadButtonDisplayed) {
  ensureSizing();

  Element* downloadButton = getElementByShadowPseudoId(
      mediaControls(), "-internal-media-controls-download-button");
  ASSERT_NE(nullptr, downloadButton);

  mediaControls().mediaElement().setSrc("https://example.com/foo.mp4");
  testing::runPendingTasks();
  simulateLoadedMetadata();

  // Download button should normally be displayed.
  EXPECT_TRUE(isElementVisible(*downloadButton));
}

TEST_F(MediaControlsImplTest, DownloadButtonNotDisplayedEmptyUrl) {
  ensureSizing();

  Element* downloadButton = getElementByShadowPseudoId(
      mediaControls(), "-internal-media-controls-download-button");
  ASSERT_NE(nullptr, downloadButton);

  // Download button should not be displayed when URL is empty.
  mediaControls().mediaElement().setSrc("");
  testing::runPendingTasks();
  simulateLoadedMetadata();
  EXPECT_FALSE(isElementVisible(*downloadButton));
}

TEST_F(MediaControlsImplTest, DownloadButtonDisplayedHiddenAndDisplayed) {
  ensureSizing();

  Element* downloadButton = getElementByShadowPseudoId(
      mediaControls(), "-internal-media-controls-download-button");
  ASSERT_NE(nullptr, downloadButton);

  // Initially show button.
  mediaControls().mediaElement().setSrc("https://example.com/foo.mp4");
  testing::runPendingTasks();
  simulateLoadedMetadata();
  EXPECT_TRUE(isElementVisible(*downloadButton));
  histogramTester().expectBucketCount("Media.Controls.Download",
                                      DownloadActionMetrics::Shown, 1);

  // Hide button.
  mediaControls().mediaElement().setSrc("");
  testing::runPendingTasks();
  EXPECT_FALSE(isElementVisible(*downloadButton));
  histogramTester().expectBucketCount("Media.Controls.Download",
                                      DownloadActionMetrics::Shown, 1);

  // Showing button again should not increment Shown count.
  mediaControls().mediaElement().setSrc("https://example.com/foo.mp4");
  testing::runPendingTasks();
  EXPECT_TRUE(isElementVisible(*downloadButton));
  histogramTester().expectBucketCount("Media.Controls.Download",
                                      DownloadActionMetrics::Shown, 1);
}

TEST_F(MediaControlsImplTest, DownloadButtonRecordsClickOnlyOnce) {
  ensureSizing();

  MediaControlDownloadButtonElement* downloadButton =
      static_cast<MediaControlDownloadButtonElement*>(
          getElementByShadowPseudoId(
              mediaControls(), "-internal-media-controls-download-button"));
  ASSERT_NE(nullptr, downloadButton);

  // Initially show button.
  mediaControls().mediaElement().setSrc("https://example.com/foo.mp4");
  testing::runPendingTasks();
  simulateLoadedMetadata();
  EXPECT_TRUE(isElementVisible(*downloadButton));
  histogramTester().expectBucketCount("Media.Controls.Download",
                                      DownloadActionMetrics::Shown, 1);

  // Click button once.
  downloadButton->dispatchSimulatedClick(
      Event::createBubble(EventTypeNames::click), SendNoEvents);
  histogramTester().expectBucketCount("Media.Controls.Download",
                                      DownloadActionMetrics::Clicked, 1);

  // Clicking button again should not increment Clicked count.
  downloadButton->dispatchSimulatedClick(
      Event::createBubble(EventTypeNames::click), SendNoEvents);
  histogramTester().expectBucketCount("Media.Controls.Download",
                                      DownloadActionMetrics::Clicked, 1);
}

TEST_F(MediaControlsImplTest, DownloadButtonNotDisplayedInfiniteDuration) {
  ensureSizing();

  Element* downloadButton = getElementByShadowPseudoId(
      mediaControls(), "-internal-media-controls-download-button");
  ASSERT_NE(nullptr, downloadButton);

  mediaControls().mediaElement().setSrc("https://example.com/foo.mp4");
  testing::runPendingTasks();

  // Download button should not be displayed when duration is infinite.
  mediaControls().mediaElement().durationChanged(
      std::numeric_limits<double>::infinity(), false /* requestSeek */);
  simulateLoadedMetadata();
  EXPECT_FALSE(isElementVisible(*downloadButton));
}

TEST_F(MediaControlsImplTest, DownloadButtonNotDisplayedHLS) {
  ensureSizing();

  Element* downloadButton = getElementByShadowPseudoId(
      mediaControls(), "-internal-media-controls-download-button");
  ASSERT_NE(nullptr, downloadButton);

  // Download button should not be displayed for HLS streams.
  mediaControls().mediaElement().setSrc("https://example.com/foo.m3u8");
  testing::runPendingTasks();
  simulateLoadedMetadata();
  EXPECT_FALSE(isElementVisible(*downloadButton));
}

TEST_F(MediaControlsImplTest, TimelineSeekToRoundedEnd) {
  ensureSizing();

  // Tests the case where the real length of the video, |exactDuration|, gets
  // rounded up slightly to |roundedUpDuration| when setting the timeline's
  // |max| attribute (crbug.com/695065).
  double exactDuration = 596.586667;
  double roundedUpDuration = 596.587;
  loadMediaWithDuration(exactDuration);

  // Simulate a click slightly past the end of the track of the timeline's
  // underlying <input type="range">. This would set the |value| to the |max|
  // attribute, which can be slightly rounded relative to the duration.
  MediaControlTimelineElement* timeline = mediaControls().timelineElement();
  timeline->setValueAsNumber(roundedUpDuration, ASSERT_NO_EXCEPTION);
  ASSERT_EQ(roundedUpDuration, timeline->valueAsNumber());
  EXPECT_EQ(0.0, mediaControls().mediaElement().currentTime());
  timeline->dispatchInputEvent();
  EXPECT_EQ(exactDuration, mediaControls().mediaElement().currentTime());
}

TEST_F(MediaControlsImplTest, TimelineImmediatelyUpdatesCurrentTime) {
  ensureSizing();

  MediaControlCurrentTimeDisplayElement* currentTimeDisplay =
      static_cast<MediaControlCurrentTimeDisplayElement*>(
          getElementByShadowPseudoId(
              mediaControls(), "-webkit-media-controls-current-time-display"));

  double duration = 600;
  loadMediaWithDuration(duration);

  // Simulate seeking the underlying range to 50%. Current time display should
  // update synchronously (rather than waiting for media to finish seeking).
  mediaControls().timelineElement()->setValueAsNumber(duration / 2,
                                                      ASSERT_NO_EXCEPTION);
  mediaControls().timelineElement()->dispatchInputEvent();
  EXPECT_EQ(duration / 2, currentTimeDisplay->currentValue());
}

TEST_F(MediaControlsImplTest, VolumeSliderPaintInvalidationOnInput) {
  ensureSizing();

  Element* volumeSlider = mediaControls().volumeSliderElement();

  MockLayoutObject layoutObject;
  LayoutObject* prevLayoutObject = volumeSlider->layoutObject();
  volumeSlider->setLayoutObject(&layoutObject);

  Event* event = Event::create(EventTypeNames::input);
  volumeSlider->defaultEventHandler(event);
  EXPECT_EQ(1, layoutObject.fullPaintInvalidationCallCount());

  event = Event::create(EventTypeNames::input);
  volumeSlider->defaultEventHandler(event);
  EXPECT_EQ(2, layoutObject.fullPaintInvalidationCallCount());

  event = Event::create(EventTypeNames::input);
  volumeSlider->defaultEventHandler(event);
  EXPECT_EQ(3, layoutObject.fullPaintInvalidationCallCount());

  volumeSlider->setLayoutObject(prevLayoutObject);
}

TEST_F(MediaControlsImplTest, TimelineMetricsWidth) {
  mediaControls().mediaElement().setSrc("https://example.com/foo.mp4");
  testing::runPendingTasks();
  setReady();
  ensureSizing();
  testing::runPendingTasks();

  MediaControlTimelineElement* timeline = mediaControls().timelineElement();
  ASSERT_TRUE(isElementVisible(*timeline));
  ASSERT_LT(0, timeline->getBoundingClientRect()->width());

  mediaControls().mediaElement().play();
  testing::runPendingTasks();

  histogramTester().expectUniqueSample(
      "Media.Timeline.Width.InlineLandscape",
      timeline->getBoundingClientRect()->width(), 1);
  histogramTester().expectTotalCount("Media.Timeline.Width.InlinePortrait", 0);
  histogramTester().expectTotalCount("Media.Timeline.Width.FullscreenLandscape",
                                     0);
  histogramTester().expectTotalCount("Media.Timeline.Width.FullscreenPortrait",
                                     0);
}

TEST_F(MediaControlsImplTest, TimelineMetricsClick) {
  double duration = 540;  // 9 minutes
  loadMediaWithDuration(duration);
  ensureSizing();
  testing::runPendingTasks();

  ASSERT_TRUE(isElementVisible(*mediaControls().timelineElement()));
  ClientRect* timelineRect =
      mediaControls().timelineElement()->getBoundingClientRect();
  ASSERT_LT(0, timelineRect->width());

  EXPECT_EQ(0, mediaControls().mediaElement().currentTime());

  WebFloatPoint trackCenter(timelineRect->left() + timelineRect->width() / 2,
                            timelineRect->top() + timelineRect->height() / 2);
  mouseDownAt(trackCenter);
  mouseUpAt(trackCenter);
  testing::runPendingTasks();

  EXPECT_LE(0.49 * duration, mediaControls().mediaElement().currentTime());
  EXPECT_GE(0.51 * duration, mediaControls().mediaElement().currentTime());

  histogramTester().expectUniqueSample("Media.Timeline.SeekType.128_255",
                                       0 /* SeekType::kClick */, 1);
  histogramTester().expectTotalCount(
      "Media.Timeline.DragGestureDuration.128_255", 0);
  histogramTester().expectTotalCount("Media.Timeline.DragPercent.128_255", 0);
  histogramTester().expectTotalCount(
      "Media.Timeline.DragSumAbsTimeDelta.128_255", 0);
  histogramTester().expectTotalCount("Media.Timeline.DragTimeDelta.128_255", 0);
}

TEST_F(MediaControlsImplTest, TimelineMetricsDragFromCurrentPosition) {
  double duration = 540;  // 9 minutes
  loadMediaWithDuration(duration);
  ensureSizing();
  testing::runPendingTasks();

  ASSERT_TRUE(isElementVisible(*mediaControls().timelineElement()));
  ClientRect* timelineRect =
      mediaControls().timelineElement()->getBoundingClientRect();
  ASSERT_LT(0, timelineRect->width());

  EXPECT_EQ(0, mediaControls().mediaElement().currentTime());

  float y = timelineRect->top() + timelineRect->height() / 2;
  WebFloatPoint thumb(timelineRect->left(), y);
  WebFloatPoint trackTwoThirds(
      timelineRect->left() + timelineRect->width() * 2 / 3, y);
  mouseDownAt(thumb);
  mouseMoveTo(trackTwoThirds);
  mouseUpAt(trackTwoThirds);

  EXPECT_LE(0.66 * duration, mediaControls().mediaElement().currentTime());
  EXPECT_GE(0.68 * duration, mediaControls().mediaElement().currentTime());

  histogramTester().expectUniqueSample(
      "Media.Timeline.SeekType.128_255",
      1 /* SeekType::kDragFromCurrentPosition */, 1);
  histogramTester().expectTotalCount(
      "Media.Timeline.DragGestureDuration.128_255", 1);
  histogramTester().expectUniqueSample("Media.Timeline.DragPercent.128_255",
                                       47 /* [60.0%, 70.0%) */, 1);
  histogramTester().expectUniqueSample(
      "Media.Timeline.DragSumAbsTimeDelta.128_255", 16 /* [4m, 8m) */, 1);
  histogramTester().expectUniqueSample("Media.Timeline.DragTimeDelta.128_255",
                                       40 /* [4m, 8m) */, 1);
}

TEST_F(MediaControlsImplTest, TimelineMetricsDragFromElsewhere) {
  double duration = 540;  // 9 minutes
  loadMediaWithDuration(duration);
  ensureSizing();
  testing::runPendingTasks();

  ASSERT_TRUE(isElementVisible(*mediaControls().timelineElement()));
  ClientRect* timelineRect =
      mediaControls().timelineElement()->getBoundingClientRect();
  ASSERT_LT(0, timelineRect->width());

  EXPECT_EQ(0, mediaControls().mediaElement().currentTime());

  float y = timelineRect->top() + timelineRect->height() / 2;
  WebFloatPoint trackOneThird(
      timelineRect->left() + timelineRect->width() * 1 / 3, y);
  WebFloatPoint trackTwoThirds(
      timelineRect->left() + timelineRect->width() * 2 / 3, y);
  mouseDownAt(trackOneThird);
  mouseMoveTo(trackTwoThirds);
  mouseUpAt(trackTwoThirds);

  EXPECT_LE(0.66 * duration, mediaControls().mediaElement().currentTime());
  EXPECT_GE(0.68 * duration, mediaControls().mediaElement().currentTime());

  histogramTester().expectUniqueSample("Media.Timeline.SeekType.128_255",
                                       2 /* SeekType::kDragFromElsewhere */, 1);
  histogramTester().expectTotalCount(
      "Media.Timeline.DragGestureDuration.128_255", 1);
  histogramTester().expectUniqueSample("Media.Timeline.DragPercent.128_255",
                                       42 /* [30.0%, 35.0%) */, 1);
  histogramTester().expectUniqueSample(
      "Media.Timeline.DragSumAbsTimeDelta.128_255", 15 /* [2m, 4m) */, 1);
  histogramTester().expectUniqueSample("Media.Timeline.DragTimeDelta.128_255",
                                       39 /* [2m, 4m) */, 1);
}

TEST_F(MediaControlsImplTest, TimelineMetricsDragBackAndForth) {
  double duration = 540;  // 9 minutes
  loadMediaWithDuration(duration);
  ensureSizing();
  testing::runPendingTasks();

  ASSERT_TRUE(isElementVisible(*mediaControls().timelineElement()));
  ClientRect* timelineRect =
      mediaControls().timelineElement()->getBoundingClientRect();
  ASSERT_LT(0, timelineRect->width());

  EXPECT_EQ(0, mediaControls().mediaElement().currentTime());

  float y = timelineRect->top() + timelineRect->height() / 2;
  WebFloatPoint trackTwoThirds(
      timelineRect->left() + timelineRect->width() * 2 / 3, y);
  WebFloatPoint trackEnd(timelineRect->left() + timelineRect->width(), y);
  WebFloatPoint trackOneThird(
      timelineRect->left() + timelineRect->width() * 1 / 3, y);
  mouseDownAt(trackTwoThirds);
  mouseMoveTo(trackEnd);
  mouseMoveTo(trackOneThird);
  mouseUpAt(trackOneThird);

  EXPECT_LE(0.32 * duration, mediaControls().mediaElement().currentTime());
  EXPECT_GE(0.34 * duration, mediaControls().mediaElement().currentTime());

  histogramTester().expectUniqueSample("Media.Timeline.SeekType.128_255",
                                       2 /* SeekType::kDragFromElsewhere */, 1);
  histogramTester().expectTotalCount(
      "Media.Timeline.DragGestureDuration.128_255", 1);
  histogramTester().expectUniqueSample("Media.Timeline.DragPercent.128_255",
                                       8 /* (-35.0%, -30.0%] */, 1);
  histogramTester().expectUniqueSample(
      "Media.Timeline.DragSumAbsTimeDelta.128_255", 17 /* [8m, 15m) */, 1);
  histogramTester().expectUniqueSample("Media.Timeline.DragTimeDelta.128_255",
                                       9 /* (-4m, -2m] */, 1);
}

}  // namespace blink

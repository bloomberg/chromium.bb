// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/shadow/MediaControls.h"

#include <limits>
#include <memory>
#include "core/HTMLNames.h"
#include "core/css/StylePropertySet.h"
#include "core/dom/Document.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/StyleEngine.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLVideoElement.h"
#include "core/html/shadow/MediaControlElementTypes.h"
#include "core/loader/EmptyClients.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/heap/Handle.h"
#include "platform/testing/EmptyWebMediaPlayer.h"
#include "platform/testing/HistogramTester.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/WebSize.h"
#include "public/platform/modules/remoteplayback/WebRemotePlaybackAvailability.h"
#include "public/platform/modules/remoteplayback/WebRemotePlaybackClient.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

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

class MediaControlsTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    m_pageHolder = DummyPageHolder::create(IntSize(800, 600), nullptr,
                                           StubLocalFrameClient::create());
    Document& document = this->document();

    document.write("<video>");
    HTMLVideoElement& video =
        toHTMLVideoElement(*document.querySelector("video"));
    m_mediaControls = video.mediaControls();

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

  MediaControls& mediaControls() { return *m_mediaControls; }
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

 private:
  std::unique_ptr<DummyPageHolder> m_pageHolder;
  Persistent<MediaControls> m_mediaControls;
  HistogramTester m_histogramTester;
};

TEST_F(MediaControlsTest, HideAndShow) {
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

TEST_F(MediaControlsTest, Reset) {
  mediaControls().mediaElement().setBooleanAttribute(HTMLNames::controlsAttr,
                                                     true);

  Element* panel = getElementByShadowPseudoId(mediaControls(),
                                              "-webkit-media-controls-panel");
  ASSERT_NE(nullptr, panel);

  ASSERT_TRUE(isElementVisible(*panel));
  mediaControls().reset();
  ASSERT_TRUE(isElementVisible(*panel));
}

TEST_F(MediaControlsTest, HideAndReset) {
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

TEST_F(MediaControlsTest, ResetDoesNotTriggerInitialLayout) {
  Document& document = this->document();
  int oldElementCount = document.styleEngine().styleForElementCount();
  // Also assert that there are no layouts yet.
  ASSERT_EQ(0, oldElementCount);
  mediaControls().reset();
  int newElementCount = document.styleEngine().styleForElementCount();
  ASSERT_EQ(oldElementCount, newElementCount);
}

TEST_F(MediaControlsTest, CastButtonRequiresRoute) {
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

TEST_F(MediaControlsTest, CastButtonDisableRemotePlaybackAttr) {
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

TEST_F(MediaControlsTest, CastOverlayDefault) {
  Element* castOverlayButton = getElementByShadowPseudoId(
      mediaControls(), "-internal-media-controls-overlay-cast-button");
  ASSERT_NE(nullptr, castOverlayButton);

  simulateRouteAvailabe();
  ASSERT_TRUE(isElementVisible(*castOverlayButton));
}

TEST_F(MediaControlsTest, CastOverlayDisableRemotePlaybackAttr) {
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

TEST_F(MediaControlsTest, CastOverlayMediaControlsDisabled) {
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

TEST_F(MediaControlsTest, KeepControlsVisibleIfOverflowListVisible) {
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

TEST_F(MediaControlsTest, DownloadButtonDisplayed) {
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

TEST_F(MediaControlsTest, DownloadButtonNotDisplayedEmptyUrl) {
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

TEST_F(MediaControlsTest, DownloadButtonDisplayedHiddenAndDisplayed) {
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

TEST_F(MediaControlsTest, DownloadButtonRecordsClickOnlyOnce) {
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

TEST_F(MediaControlsTest, DownloadButtonNotDisplayedInfiniteDuration) {
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

TEST_F(MediaControlsTest, DownloadButtonNotDisplayedHLS) {
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

TEST_F(MediaControlsTest, TimelineSeekToRoundedEnd) {
  ensureSizing();

  MediaControlTimelineElement* timeline =
      static_cast<MediaControlTimelineElement*>(getElementByShadowPseudoId(
          mediaControls(), "-webkit-media-controls-timeline"));
  ASSERT_NE(nullptr, timeline);

  // Tests the case where the real length of the video, |exactDuration|, gets
  // rounded up slightly to |roundedUpDuration| when setting the timeline's
  // |max| attribute (crbug.com/695065).
  double exactDuration = 596.586667;
  double roundedUpDuration = 596.587;
  loadMediaWithDuration(exactDuration);

  // Simulate a click slightly past the end of the track of the timeline's
  // underlying <input type="range">. This would set the |value| to the |max|
  // attribute, which can be slightly rounded relative to the duration.
  timeline->setValueAsNumber(roundedUpDuration, ASSERT_NO_EXCEPTION);
  ASSERT_EQ(roundedUpDuration, timeline->valueAsNumber());
  EXPECT_EQ(0.0, mediaControls().mediaElement().currentTime());
  timeline->dispatchInputEvent();
  EXPECT_EQ(exactDuration, mediaControls().mediaElement().currentTime());
}

TEST_F(MediaControlsTest, TimelineImmediatelyUpdatesCurrentTime) {
  ensureSizing();

  MediaControlTimelineElement* timeline =
      static_cast<MediaControlTimelineElement*>(getElementByShadowPseudoId(
          mediaControls(), "-webkit-media-controls-timeline"));
  ASSERT_NE(nullptr, timeline);
  MediaControlCurrentTimeDisplayElement* currentTimeDisplay =
      static_cast<MediaControlCurrentTimeDisplayElement*>(
          getElementByShadowPseudoId(
              mediaControls(), "-webkit-media-controls-current-time-display"));
  ASSERT_NE(nullptr, currentTimeDisplay);

  double duration = 600;
  loadMediaWithDuration(duration);

  // Simulate seeking the underlying range to 50%. Current time display should
  // update synchronously (rather than waiting for media to finish seeking).
  timeline->setValueAsNumber(duration / 2, ASSERT_NO_EXCEPTION);
  timeline->dispatchInputEvent();
  EXPECT_EQ(duration / 2, currentTimeDisplay->currentValue());
}

}  // namespace blink

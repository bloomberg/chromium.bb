// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/HTMLMediaElement.h"

#include "core/dom/DocumentUserGestureToken.h"
#include "core/dom/Fullscreen.h"
#include "core/html/HTMLVideoElement.h"
#include "core/html/MediaCustomControlsFullscreenDetector.h"
#include "core/html/shadow/MediaControls.h"
#include "core/loader/EmptyClients.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/UserGestureIndicator.h"
#include "platform/testing/EmptyWebMediaPlayer.h"
#include "platform/testing/UnitTestHelpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

class MockWebMediaPlayer final : public EmptyWebMediaPlayer {
 public:
  MOCK_METHOD1(setIsEffectivelyFullscreen, void(bool));
};

class StubLocalFrameClient : public EmptyLocalFrameClient {
 public:
  static StubLocalFrameClient* create() { return new StubLocalFrameClient; }

  std::unique_ptr<WebMediaPlayer> createWebMediaPlayer(
      HTMLMediaElement&,
      const WebMediaPlayerSource&,
      WebMediaPlayerClient*) override {
    return WTF::wrapUnique(new MockWebMediaPlayer());
  }
};

using ::testing::_;
using ::testing::Invoke;

}  // anonymous namespace

class HTMLMediaElementEventListenersTest : public ::testing::Test {
 protected:
  void SetUp() override {
    m_pageHolder = DummyPageHolder::create(IntSize(800, 600), nullptr,
                                           StubLocalFrameClient::create());
  }

  Document& document() { return m_pageHolder->document(); }
  void destroyDocument() { m_pageHolder.reset(); }
  HTMLVideoElement* video() {
    return toHTMLVideoElement(document().querySelector("video"));
  }
  MockWebMediaPlayer* webMediaPlayer() {
    return static_cast<MockWebMediaPlayer*>(video()->webMediaPlayer());
  }
  MediaControls* controls() { return video()->mediaControls(); }
  void simulateReadyState(HTMLMediaElement::ReadyState state) {
    video()->setReadyState(state);
  }
  MediaCustomControlsFullscreenDetector* fullscreenDetector() {
    return video()->m_customControlsFullscreenDetector;
  }
  bool isCheckViewportIntersectionTimerActive(
      MediaCustomControlsFullscreenDetector* detector) {
    return detector->m_checkViewportIntersectionTimer.isActive();
  }

 private:
  std::unique_ptr<DummyPageHolder> m_pageHolder;
};

TEST_F(HTMLMediaElementEventListenersTest, RemovingFromDocumentCollectsAll) {
  EXPECT_EQ(video(), nullptr);
  document().body()->setInnerHTML("<body><video controls></video></body>");
  EXPECT_NE(video(), nullptr);
  EXPECT_TRUE(video()->hasEventListeners());
  EXPECT_NE(controls(), nullptr);
  EXPECT_TRUE(document().hasEventListeners());

  WeakPersistent<HTMLVideoElement> weakPersistentVideo = video();
  WeakPersistent<MediaControls> weakPersistentControls = controls();
  {
    Persistent<HTMLVideoElement> persistentVideo = video();
    document().body()->setInnerHTML("");

    // When removed from the document, the event listeners should have been
    // dropped.
    EXPECT_FALSE(document().hasEventListeners());
    // The video element should still have some event listeners.
    EXPECT_TRUE(persistentVideo->hasEventListeners());
  }

  ThreadState::current()->collectAllGarbage();

  // They have been GC'd.
  EXPECT_EQ(weakPersistentVideo, nullptr);
  EXPECT_EQ(weakPersistentControls, nullptr);
}

TEST_F(HTMLMediaElementEventListenersTest,
       ReInsertingInDocumentCollectsControls) {
  EXPECT_EQ(video(), nullptr);
  document().body()->setInnerHTML("<body><video controls></video></body>");
  EXPECT_NE(video(), nullptr);
  EXPECT_TRUE(video()->hasEventListeners());
  EXPECT_NE(controls(), nullptr);
  EXPECT_TRUE(document().hasEventListeners());

  // This should be a no-op. We keep a reference on the VideoElement to avoid an
  // unexpected GC.
  {
    Persistent<HTMLVideoElement> videoHolder = video();
    document().body()->removeChild(video());
    document().body()->appendChild(videoHolder.get());
  }

  EXPECT_TRUE(document().hasEventListeners());
  EXPECT_TRUE(video()->hasEventListeners());

  ThreadState::current()->collectAllGarbage();

  EXPECT_NE(video(), nullptr);
  EXPECT_NE(controls(), nullptr);
  EXPECT_EQ(controls(), video()->mediaControls());
}

TEST_F(HTMLMediaElementEventListenersTest,
       FullscreenDetectorTimerCancelledOnContextDestroy) {
  bool originalVideoFullscreenDetectionEnabled =
      RuntimeEnabledFeatures::videoFullscreenDetectionEnabled();

  RuntimeEnabledFeatures::setVideoFullscreenDetectionEnabled(true);

  EXPECT_EQ(video(), nullptr);
  document().body()->setInnerHTML("<body><video></video</body>");
  video()->setSrc("http://example.com");

  testing::runPendingTasks();

  EXPECT_NE(webMediaPlayer(), nullptr);

  // Set ReadyState as HaveMetadata and go fullscreen, so the timer is fired.
  EXPECT_NE(video(), nullptr);
  simulateReadyState(HTMLMediaElement::kHaveMetadata);
  UserGestureIndicator gestureIndicator(
      DocumentUserGestureToken::create(&document()));
  Fullscreen::requestFullscreen(*video());
  Fullscreen::from(document()).didEnterFullscreen();

  testing::runPendingTasks();

  Persistent<Document> persistentDocument = &document();
  Persistent<MediaCustomControlsFullscreenDetector> detector =
      fullscreenDetector();

  std::vector<bool> observedResults;

  ON_CALL(*webMediaPlayer(), setIsEffectivelyFullscreen(_))
      .WillByDefault(Invoke(
          [&](bool isFullscreen) { observedResults.push_back(isFullscreen); }));

  destroyDocument();

  testing::runPendingTasks();

  // Document should not have listeners as the ExecutionContext is destroyed.
  EXPECT_FALSE(persistentDocument->hasEventListeners());
  // The timer should be cancelled when the ExecutionContext is destroyed.
  EXPECT_FALSE(isCheckViewportIntersectionTimerActive(detector));
  // Should only notify the false value when ExecutionContext is destroyed.
  EXPECT_EQ(1u, observedResults.size());
  EXPECT_FALSE(observedResults[0]);

  RuntimeEnabledFeatures::setVideoFullscreenDetectionEnabled(
      originalVideoFullscreenDetectionEnabled);
}

}  // namespace blink

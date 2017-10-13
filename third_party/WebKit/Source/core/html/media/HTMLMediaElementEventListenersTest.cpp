// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/media/HTMLMediaElement.h"

#include "core/dom/UserGestureIndicator.h"
#include "core/fullscreen/Fullscreen.h"
#include "core/html/media/HTMLVideoElement.h"
#include "core/html/media/MediaControls.h"
#include "core/html/media/MediaCustomControlsFullscreenDetector.h"
#include "core/loader/EmptyClients.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/testing/EmptyWebMediaPlayer.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

class MockWebMediaPlayer final : public EmptyWebMediaPlayer {
 public:
  MOCK_METHOD1(SetIsEffectivelyFullscreen, void(bool));
};

class MediaStubLocalFrameClient : public EmptyLocalFrameClient {
 public:
  static MediaStubLocalFrameClient* Create() {
    return new MediaStubLocalFrameClient;
  }

  std::unique_ptr<WebMediaPlayer> CreateWebMediaPlayer(
      HTMLMediaElement&,
      const WebMediaPlayerSource&,
      WebMediaPlayerClient*,
      WebLayerTreeView*) override {
    return WTF::WrapUnique(new MockWebMediaPlayer());
  }
};

using ::testing::_;
using ::testing::Invoke;

}  // anonymous namespace

class HTMLMediaElementEventListenersTest : public ::testing::Test {
 protected:
  void SetUp() override {
    page_holder_ = DummyPageHolder::Create(IntSize(800, 600), nullptr,
                                           MediaStubLocalFrameClient::Create());
  }

  Document& GetDocument() { return page_holder_->GetDocument(); }
  void DestroyDocument() { page_holder_.reset(); }
  HTMLVideoElement* Video() {
    return ToHTMLVideoElement(GetDocument().QuerySelector("video"));
  }
  MockWebMediaPlayer* WebMediaPlayer() {
    return static_cast<MockWebMediaPlayer*>(Video()->GetWebMediaPlayer());
  }
  MediaControls* Controls() { return Video()->GetMediaControls(); }
  void SimulateReadyState(HTMLMediaElement::ReadyState state) {
    Video()->SetReadyState(state);
  }
  MediaCustomControlsFullscreenDetector* FullscreenDetector() {
    return Video()->custom_controls_fullscreen_detector_;
  }
  bool IsCheckViewportIntersectionTimerActive(
      MediaCustomControlsFullscreenDetector* detector) {
    return detector->check_viewport_intersection_timer_.IsActive();
  }

 private:
  std::unique_ptr<DummyPageHolder> page_holder_;
};

TEST_F(HTMLMediaElementEventListenersTest, RemovingFromDocumentCollectsAll) {
  EXPECT_EQ(Video(), nullptr);
  GetDocument().body()->SetInnerHTMLFromString(
      "<body><video controls></video></body>");
  EXPECT_NE(Video(), nullptr);
  EXPECT_TRUE(Video()->HasEventListeners());
  EXPECT_NE(Controls(), nullptr);
  EXPECT_TRUE(GetDocument().HasEventListeners());

  WeakPersistent<HTMLVideoElement> weak_persistent_video = Video();
  WeakPersistent<MediaControls> weak_persistent_controls = Controls();
  {
    Persistent<HTMLVideoElement> persistent_video = Video();
    GetDocument().body()->SetInnerHTMLFromString("");

    // When removed from the document, the event listeners should have been
    // dropped.
    EXPECT_FALSE(GetDocument().HasEventListeners());
    // The video element should still have some event listeners.
    EXPECT_TRUE(persistent_video->HasEventListeners());
  }

  testing::RunPendingTasks();

  ThreadState::Current()->CollectAllGarbage();

  // They have been GC'd.
  EXPECT_EQ(weak_persistent_video, nullptr);
  EXPECT_EQ(weak_persistent_controls, nullptr);
}

TEST_F(HTMLMediaElementEventListenersTest,
       ReInsertingInDocumentCollectsControls) {
  EXPECT_EQ(Video(), nullptr);
  GetDocument().body()->SetInnerHTMLFromString(
      "<body><video controls></video></body>");
  EXPECT_NE(Video(), nullptr);
  EXPECT_TRUE(Video()->HasEventListeners());
  EXPECT_NE(Controls(), nullptr);
  EXPECT_TRUE(GetDocument().HasEventListeners());

  // This should be a no-op. We keep a reference on the VideoElement to avoid an
  // unexpected GC.
  {
    Persistent<HTMLVideoElement> video_holder = Video();
    GetDocument().body()->RemoveChild(Video());
    GetDocument().body()->AppendChild(video_holder.Get());
  }

  EXPECT_TRUE(GetDocument().HasEventListeners());
  EXPECT_TRUE(Video()->HasEventListeners());

  testing::RunPendingTasks();

  ThreadState::Current()->CollectAllGarbage();

  EXPECT_NE(Video(), nullptr);
  EXPECT_NE(Controls(), nullptr);
  EXPECT_EQ(Controls(), Video()->GetMediaControls());
}

TEST_F(HTMLMediaElementEventListenersTest,
       FullscreenDetectorTimerCancelledOnContextDestroy) {
  ScopedVideoFullscreenDetectionForTest video_fullscreen_detection(true);

  EXPECT_EQ(Video(), nullptr);
  GetDocument().body()->SetInnerHTMLFromString("<body><video></video</body>");
  Video()->SetSrc("http://example.com");

  testing::RunPendingTasks();

  EXPECT_NE(WebMediaPlayer(), nullptr);

  // Set ReadyState as HaveMetadata and go fullscreen, so the timer is fired.
  EXPECT_NE(Video(), nullptr);
  SimulateReadyState(HTMLMediaElement::kHaveMetadata);
  std::unique_ptr<UserGestureIndicator> gesture_indicator =
      LocalFrame::CreateUserGesture(GetDocument().GetFrame());
  Fullscreen::RequestFullscreen(*Video());
  Fullscreen::From(GetDocument()).DidEnterFullscreen();

  testing::RunPendingTasks();

  Persistent<Document> persistent_document = &GetDocument();
  Persistent<MediaCustomControlsFullscreenDetector> detector =
      FullscreenDetector();

  std::vector<bool> observed_results;

  ON_CALL(*WebMediaPlayer(), SetIsEffectivelyFullscreen(_))
      .WillByDefault(Invoke([&](bool is_fullscreen) {
        observed_results.push_back(is_fullscreen);
      }));

  DestroyDocument();

  testing::RunPendingTasks();

  // Document should not have listeners as the ExecutionContext is destroyed.
  EXPECT_FALSE(persistent_document->HasEventListeners());
  // The timer should be cancelled when the ExecutionContext is destroyed.
  EXPECT_FALSE(IsCheckViewportIntersectionTimerActive(detector));
  // Should only notify the false value when ExecutionContext is destroyed.
  EXPECT_EQ(1u, observed_results.size());
  EXPECT_FALSE(observed_results[0]);
}

}  // namespace blink

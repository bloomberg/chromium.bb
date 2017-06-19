// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/HTMLVideoElement.h"

#include <memory>

#include "core/dom/Fullscreen.h"
#include "core/dom/UserGestureIndicator.h"
#include "core/html/HTMLDivElement.h"
#include "core/layout/LayoutFullScreen.h"
#include "core/loader/EmptyClients.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/testing/UnitTestHelpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

class MockChromeClient : public EmptyChromeClient {
 public:
  MOCK_METHOD1(EnterFullscreen, void(LocalFrame&));
  MOCK_METHOD1(ExitFullscreen, void(LocalFrame&));
};

using ::testing::_;
using ::testing::Sequence;

}  // anonymous namespace

class HTMLVideoElementPersistentTest : public ::testing::Test {
 protected:
  void SetUp() override {
    chrome_client_ = new MockChromeClient();

    Page::PageClients clients;
    FillWithEmptyClients(clients);
    clients.chrome_client = chrome_client_.Get();

    page_holder_ = DummyPageHolder::Create(IntSize(800, 600), &clients);
    GetDocument().body()->setInnerHTML(
        "<body><div><video></video></div></body>");
  }

  Document& GetDocument() { return page_holder_->GetDocument(); }

  HTMLVideoElement* VideoElement() {
    return toHTMLVideoElement(GetDocument().QuerySelector("video"));
  }

  HTMLDivElement* DivElement() {
    return toHTMLDivElement(GetDocument().QuerySelector("div"));
  }

  Element* FullscreenElement() {
    return Fullscreen::FullscreenElementFrom(GetDocument());
  }

  MockChromeClient& GetMockChromeClient() { return *chrome_client_; }

  void SimulateDidEnterFullscreen() {
    Fullscreen::FromIfExists(GetDocument())->DidEnterFullscreen();
  }

  void SimulateDidExitFullscreen() {
    Fullscreen::FromIfExists(GetDocument())->DidExitFullscreen();
  }

  void SimulateBecamePersistentVideo(bool value) {
    VideoElement()->OnBecamePersistentVideo(value);
  }

 private:
  std::unique_ptr<DummyPageHolder> page_holder_;
  Persistent<MockChromeClient> chrome_client_;
};

TEST_F(HTMLVideoElementPersistentTest, nothingIsFullscreen) {
  Sequence s;

  EXPECT_EQ(FullscreenElement(), nullptr);

  // Making the video persistent should be a no-op.
  SimulateBecamePersistentVideo(true);
  EXPECT_EQ(FullscreenElement(), nullptr);
  EXPECT_FALSE(VideoElement()->IsPersistent());
  EXPECT_FALSE(DivElement()->ContainsPersistentVideo());
  EXPECT_FALSE(VideoElement()->ContainsPersistentVideo());

  // Making the video not persitent should also be a no-op.
  SimulateBecamePersistentVideo(false);
  EXPECT_EQ(FullscreenElement(), nullptr);
  EXPECT_FALSE(VideoElement()->IsPersistent());
  EXPECT_FALSE(DivElement()->ContainsPersistentVideo());
  EXPECT_FALSE(VideoElement()->ContainsPersistentVideo());
}

TEST_F(HTMLVideoElementPersistentTest, videoIsFullscreen) {
  EXPECT_EQ(FullscreenElement(), nullptr);

  EXPECT_CALL(GetMockChromeClient(), EnterFullscreen(_)).Times(1);
  EXPECT_CALL(GetMockChromeClient(), ExitFullscreen(_)).Times(0);

  UserGestureIndicator gesture_indicator(
      UserGestureToken::Create(&GetDocument()));
  Fullscreen::RequestFullscreen(*VideoElement());
  SimulateDidEnterFullscreen();
  EXPECT_EQ(FullscreenElement(), VideoElement());

  // This should be no-op.
  SimulateBecamePersistentVideo(true);
  EXPECT_EQ(FullscreenElement(), VideoElement());
  EXPECT_FALSE(VideoElement()->IsPersistent());
  EXPECT_FALSE(DivElement()->ContainsPersistentVideo());
  EXPECT_FALSE(VideoElement()->ContainsPersistentVideo());

  // This should be no-op.
  SimulateBecamePersistentVideo(false);
  EXPECT_EQ(FullscreenElement(), VideoElement());
  EXPECT_FALSE(VideoElement()->IsPersistent());
  EXPECT_FALSE(DivElement()->ContainsPersistentVideo());
  EXPECT_FALSE(VideoElement()->ContainsPersistentVideo());
}

TEST_F(HTMLVideoElementPersistentTest, divIsFullscreen) {
  EXPECT_EQ(FullscreenElement(), nullptr);

  EXPECT_CALL(GetMockChromeClient(), EnterFullscreen(_)).Times(1);
  EXPECT_CALL(GetMockChromeClient(), ExitFullscreen(_)).Times(0);

  UserGestureIndicator gesture_indicator(
      UserGestureToken::Create(&GetDocument()));
  Fullscreen::RequestFullscreen(*DivElement());
  SimulateDidEnterFullscreen();
  EXPECT_EQ(FullscreenElement(), DivElement());

  // Make the video persistent.
  SimulateBecamePersistentVideo(true);
  EXPECT_EQ(FullscreenElement(), DivElement());
  EXPECT_TRUE(VideoElement()->IsPersistent());
  EXPECT_TRUE(DivElement()->ContainsPersistentVideo());
  EXPECT_TRUE(VideoElement()->ContainsPersistentVideo());

  // This should be no-op.
  SimulateBecamePersistentVideo(true);
  EXPECT_EQ(FullscreenElement(), DivElement());
  EXPECT_TRUE(VideoElement()->IsPersistent());
  EXPECT_TRUE(DivElement()->ContainsPersistentVideo());
  EXPECT_TRUE(VideoElement()->ContainsPersistentVideo());

  // Make the video not persistent.
  SimulateBecamePersistentVideo(false);
  EXPECT_EQ(FullscreenElement(), DivElement());
  EXPECT_FALSE(VideoElement()->IsPersistent());
  EXPECT_FALSE(DivElement()->ContainsPersistentVideo());
  EXPECT_FALSE(VideoElement()->ContainsPersistentVideo());
}

TEST_F(HTMLVideoElementPersistentTest, exitFullscreenBeforePersistence) {
  EXPECT_EQ(FullscreenElement(), nullptr);

  EXPECT_CALL(GetMockChromeClient(), EnterFullscreen(_)).Times(1);
  EXPECT_CALL(GetMockChromeClient(), ExitFullscreen(_)).Times(1);

  UserGestureIndicator gesture_indicator(
      UserGestureToken::Create(&GetDocument()));
  Fullscreen::RequestFullscreen(*DivElement());
  SimulateDidEnterFullscreen();
  EXPECT_EQ(FullscreenElement(), DivElement());

  SimulateBecamePersistentVideo(true);

  Fullscreen::FullyExitFullscreen(GetDocument());
  SimulateDidExitFullscreen();
  EXPECT_EQ(FullscreenElement(), nullptr);

  // Video persistence states should still apply.
  EXPECT_TRUE(VideoElement()->IsPersistent());
  EXPECT_TRUE(DivElement()->ContainsPersistentVideo());
  EXPECT_TRUE(VideoElement()->ContainsPersistentVideo());

  // Make the video not persistent, cleaned up.
  SimulateBecamePersistentVideo(false);
  EXPECT_FALSE(VideoElement()->IsPersistent());
  EXPECT_FALSE(DivElement()->ContainsPersistentVideo());
  EXPECT_FALSE(VideoElement()->ContainsPersistentVideo());
}

TEST_F(HTMLVideoElementPersistentTest, internalPseudoClassOnlyUAStyleSheet) {
  EXPECT_EQ(FullscreenElement(), nullptr);

  EXPECT_CALL(GetMockChromeClient(), EnterFullscreen(_)).Times(1);
  EXPECT_CALL(GetMockChromeClient(), ExitFullscreen(_)).Times(0);

  DummyExceptionStateForTesting exception_state;

  EXPECT_FALSE(DivElement()->matches(":-webkit-full-screen"));
  EXPECT_FALSE(DivElement()->matches(":-internal-video-persistent-ancestor",
                                     exception_state));
  EXPECT_TRUE(exception_state.HadException());
  exception_state.ClearException();
  EXPECT_FALSE(
      VideoElement()->matches(":-internal-video-persistent", exception_state));
  EXPECT_TRUE(exception_state.HadException());
  exception_state.ClearException();
  EXPECT_FALSE(VideoElement()->matches(":-internal-video-persistent-ancestor",
                                       exception_state));
  EXPECT_TRUE(exception_state.HadException());
  exception_state.ClearException();

  UserGestureIndicator gesture_indicator(
      UserGestureToken::Create(&GetDocument()));
  Fullscreen::RequestFullscreen(*DivElement());
  SimulateDidEnterFullscreen();
  SimulateBecamePersistentVideo(true);

  EXPECT_EQ(FullscreenElement(), DivElement());
  EXPECT_TRUE(VideoElement()->IsPersistent());
  EXPECT_TRUE(DivElement()->ContainsPersistentVideo());
  EXPECT_TRUE(VideoElement()->ContainsPersistentVideo());

  // The :internal-* rules apply only from the UA stylesheet.
  EXPECT_TRUE(DivElement()->matches(":-webkit-full-screen"));
  EXPECT_FALSE(DivElement()->matches(":-internal-video-persistent-ancestor",
                                     exception_state));
  EXPECT_TRUE(exception_state.HadException());
  exception_state.ClearException();
  EXPECT_FALSE(
      VideoElement()->matches(":-internal-video-persistent", exception_state));
  EXPECT_TRUE(exception_state.HadException());
  exception_state.ClearException();
  EXPECT_FALSE(VideoElement()->matches(":-internal-video-persistent-ancestor",
                                       exception_state));
  EXPECT_TRUE(exception_state.HadException());
  exception_state.ClearException();
}

TEST_F(HTMLVideoElementPersistentTest, removeContainerWhilePersisting) {
  EXPECT_EQ(FullscreenElement(), nullptr);

  EXPECT_CALL(GetMockChromeClient(), EnterFullscreen(_)).Times(1);
  EXPECT_CALL(GetMockChromeClient(), ExitFullscreen(_)).Times(1);

  UserGestureIndicator gesture_indicator(
      UserGestureToken::Create(&GetDocument()));
  Fullscreen::RequestFullscreen(*DivElement());
  SimulateDidEnterFullscreen();
  EXPECT_EQ(FullscreenElement(), DivElement());

  SimulateBecamePersistentVideo(true);
  Persistent<HTMLDivElement> div = DivElement();
  Persistent<HTMLVideoElement> video = VideoElement();
  GetDocument().body()->RemoveChild(DivElement());

  EXPECT_FALSE(video->IsPersistent());
  EXPECT_FALSE(div->ContainsPersistentVideo());
  EXPECT_FALSE(video->ContainsPersistentVideo());
}

TEST_F(HTMLVideoElementPersistentTest, removeVideoWhilePersisting) {
  EXPECT_EQ(FullscreenElement(), nullptr);

  EXPECT_CALL(GetMockChromeClient(), EnterFullscreen(_)).Times(1);
  EXPECT_CALL(GetMockChromeClient(), ExitFullscreen(_)).Times(0);

  UserGestureIndicator gesture_indicator(
      UserGestureToken::Create(&GetDocument()));
  Fullscreen::RequestFullscreen(*DivElement());
  SimulateDidEnterFullscreen();
  EXPECT_EQ(FullscreenElement(), DivElement());

  SimulateBecamePersistentVideo(true);
  Persistent<HTMLVideoElement> video = VideoElement();
  DivElement()->RemoveChild(VideoElement());

  EXPECT_FALSE(video->IsPersistent());
  EXPECT_FALSE(DivElement()->ContainsPersistentVideo());
  EXPECT_FALSE(video->ContainsPersistentVideo());
}

TEST_F(HTMLVideoElementPersistentTest, removeVideoWithLayerWhilePersisting) {
  EXPECT_EQ(FullscreenElement(), nullptr);

  // Inserting a <span> between the <div> and <video>.
  Persistent<Element> span = GetDocument().createElement("span");
  DivElement()->AppendChild(span);
  span->AppendChild(VideoElement());

  EXPECT_CALL(GetMockChromeClient(), EnterFullscreen(_)).Times(1);
  EXPECT_CALL(GetMockChromeClient(), ExitFullscreen(_)).Times(0);

  UserGestureIndicator gesture_indicator(
      UserGestureToken::Create(&GetDocument()));
  Fullscreen::RequestFullscreen(*DivElement());
  SimulateDidEnterFullscreen();
  EXPECT_EQ(FullscreenElement(), DivElement());

  SimulateBecamePersistentVideo(true);
  Persistent<HTMLVideoElement> video = VideoElement();
  span->RemoveChild(VideoElement());

  EXPECT_FALSE(video->IsPersistent());
  EXPECT_FALSE(DivElement()->ContainsPersistentVideo());
  EXPECT_FALSE(video->ContainsPersistentVideo());
  EXPECT_FALSE(span->ContainsPersistentVideo());
}

TEST_F(HTMLVideoElementPersistentTest, containsPersistentVideoScopedToFS) {
  EXPECT_EQ(FullscreenElement(), nullptr);

  EXPECT_CALL(GetMockChromeClient(), EnterFullscreen(_)).Times(1);
  EXPECT_CALL(GetMockChromeClient(), ExitFullscreen(_)).Times(0);

  UserGestureIndicator gesture_indicator(
      UserGestureToken::Create(&GetDocument()));
  Fullscreen::RequestFullscreen(*DivElement());
  SimulateDidEnterFullscreen();
  EXPECT_EQ(FullscreenElement(), DivElement());

  SimulateBecamePersistentVideo(true);
  EXPECT_FALSE(GetDocument().body()->ContainsPersistentVideo());
}

}  // namespace blink

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/HTMLVideoElement.h"

#include <memory>

#include "core/dom/DocumentUserGestureToken.h"
#include "core/dom/Fullscreen.h"
#include "core/html/HTMLDivElement.h"
#include "core/layout/LayoutFullScreen.h"
#include "core/loader/EmptyClients.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/UserGestureIndicator.h"
#include "platform/testing/UnitTestHelpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

class MockChromeClient : public EmptyChromeClient {
 public:
  MOCK_METHOD1(enterFullscreen, void(LocalFrame&));
  MOCK_METHOD1(exitFullscreen, void(LocalFrame&));
};

using ::testing::_;
using ::testing::Sequence;

}  // anonymous namespace

class HTMLVideoElementPersistentTest : public ::testing::Test {
 protected:
  void SetUp() override {
    m_chromeClient = new MockChromeClient();

    Page::PageClients clients;
    fillWithEmptyClients(clients);
    clients.chromeClient = m_chromeClient.get();

    m_pageHolder = DummyPageHolder::create(IntSize(800, 600), &clients);
    document().body()->setInnerHTML("<body><div><video></video></div></body>");
  }

  Document& document() { return m_pageHolder->document(); }

  HTMLVideoElement* videoElement() {
    return toHTMLVideoElement(document().querySelector("video"));
  }

  HTMLDivElement* divElement() {
    return toHTMLDivElement(document().querySelector("div"));
  }

  Element* fullscreenElement() {
    return Fullscreen::currentFullScreenElementFrom(document());
  }

  MockChromeClient& mockChromeClient() { return *m_chromeClient; }

  void simulateDidEnterFullscreen() {
    Fullscreen::fromIfExists(document())->didEnterFullscreen();
  }

  void simulateDidExitFullscreen() {
    Fullscreen::fromIfExists(document())->didExitFullscreen();
  }

  void simulateBecamePersistentVideo(bool value) {
    videoElement()->onBecamePersistentVideo(value);
  }

 private:
  std::unique_ptr<DummyPageHolder> m_pageHolder;
  Persistent<MockChromeClient> m_chromeClient;
};

TEST_F(HTMLVideoElementPersistentTest, nothingIsFullscreen) {
  Sequence s;

  EXPECT_EQ(fullscreenElement(), nullptr);

  // Making the video persistent should be a no-op.
  simulateBecamePersistentVideo(true);
  EXPECT_EQ(fullscreenElement(), nullptr);
  EXPECT_FALSE(videoElement()->isPersistent());
  EXPECT_FALSE(divElement()->containsPersistentVideo());
  EXPECT_FALSE(videoElement()->containsPersistentVideo());

  // Making the video not persitent should also be a no-op.
  simulateBecamePersistentVideo(false);
  EXPECT_EQ(fullscreenElement(), nullptr);
  EXPECT_FALSE(videoElement()->isPersistent());
  EXPECT_FALSE(divElement()->containsPersistentVideo());
  EXPECT_FALSE(videoElement()->containsPersistentVideo());
}

TEST_F(HTMLVideoElementPersistentTest, videoIsFullscreen) {
  EXPECT_EQ(fullscreenElement(), nullptr);

  EXPECT_CALL(mockChromeClient(), enterFullscreen(_)).Times(1);
  EXPECT_CALL(mockChromeClient(), exitFullscreen(_)).Times(0);

  UserGestureIndicator gestureIndicator(
      DocumentUserGestureToken::create(&document()));
  Fullscreen::requestFullscreen(*videoElement());
  simulateDidEnterFullscreen();
  EXPECT_EQ(fullscreenElement(), videoElement());

  // This should be no-op.
  simulateBecamePersistentVideo(true);
  EXPECT_EQ(fullscreenElement(), videoElement());
  EXPECT_FALSE(videoElement()->isPersistent());
  EXPECT_FALSE(divElement()->containsPersistentVideo());
  EXPECT_FALSE(videoElement()->containsPersistentVideo());

  // This should be no-op.
  simulateBecamePersistentVideo(false);
  EXPECT_EQ(fullscreenElement(), videoElement());
  EXPECT_FALSE(videoElement()->isPersistent());
  EXPECT_FALSE(divElement()->containsPersistentVideo());
  EXPECT_FALSE(videoElement()->containsPersistentVideo());
}

TEST_F(HTMLVideoElementPersistentTest, divIsFullscreen) {
  EXPECT_EQ(fullscreenElement(), nullptr);

  EXPECT_CALL(mockChromeClient(), enterFullscreen(_)).Times(1);
  EXPECT_CALL(mockChromeClient(), exitFullscreen(_)).Times(0);

  UserGestureIndicator gestureIndicator(
      DocumentUserGestureToken::create(&document()));
  Fullscreen::requestFullscreen(*divElement());
  simulateDidEnterFullscreen();
  EXPECT_EQ(fullscreenElement(), divElement());

  // Make the video persistent.
  simulateBecamePersistentVideo(true);
  EXPECT_EQ(fullscreenElement(), divElement());
  EXPECT_TRUE(videoElement()->isPersistent());
  EXPECT_TRUE(divElement()->containsPersistentVideo());
  EXPECT_TRUE(videoElement()->containsPersistentVideo());

  // This should be no-op.
  simulateBecamePersistentVideo(true);
  EXPECT_EQ(fullscreenElement(), divElement());
  EXPECT_TRUE(videoElement()->isPersistent());
  EXPECT_TRUE(divElement()->containsPersistentVideo());
  EXPECT_TRUE(videoElement()->containsPersistentVideo());

  // Make the video not persistent.
  simulateBecamePersistentVideo(false);
  EXPECT_EQ(fullscreenElement(), divElement());
  EXPECT_FALSE(videoElement()->isPersistent());
  EXPECT_FALSE(divElement()->containsPersistentVideo());
  EXPECT_FALSE(videoElement()->containsPersistentVideo());
}

TEST_F(HTMLVideoElementPersistentTest, exitFullscreenBeforePersistence) {
  EXPECT_EQ(fullscreenElement(), nullptr);

  EXPECT_CALL(mockChromeClient(), enterFullscreen(_)).Times(1);
  EXPECT_CALL(mockChromeClient(), exitFullscreen(_)).Times(1);

  UserGestureIndicator gestureIndicator(
      DocumentUserGestureToken::create(&document()));
  Fullscreen::requestFullscreen(*divElement());
  simulateDidEnterFullscreen();
  EXPECT_EQ(fullscreenElement(), divElement());

  simulateBecamePersistentVideo(true);

  Fullscreen::fullyExitFullscreen(document());
  simulateDidExitFullscreen();
  EXPECT_EQ(fullscreenElement(), nullptr);

  // Video persistence states should still apply.
  EXPECT_TRUE(videoElement()->isPersistent());
  EXPECT_TRUE(divElement()->containsPersistentVideo());
  EXPECT_TRUE(videoElement()->containsPersistentVideo());

  // Make the video not persistent, cleaned up.
  simulateBecamePersistentVideo(false);
  EXPECT_FALSE(videoElement()->isPersistent());
  EXPECT_FALSE(divElement()->containsPersistentVideo());
  EXPECT_FALSE(videoElement()->containsPersistentVideo());
}

TEST_F(HTMLVideoElementPersistentTest, internalPseudoClassOnlyUAStyleSheet) {
  EXPECT_EQ(fullscreenElement(), nullptr);

  EXPECT_CALL(mockChromeClient(), enterFullscreen(_)).Times(1);
  EXPECT_CALL(mockChromeClient(), exitFullscreen(_)).Times(0);

  EXPECT_FALSE(divElement()->matches(":-webkit-full-screen"));
  EXPECT_FALSE(divElement()->matches(":-internal-video-persistent-ancestor"));
  EXPECT_FALSE(videoElement()->matches(":-internal-video-persistent"));
  EXPECT_FALSE(videoElement()->matches(":-internal-video-persistent-ancestor"));

  UserGestureIndicator gestureIndicator(
      DocumentUserGestureToken::create(&document()));
  Fullscreen::requestFullscreen(*divElement());
  simulateDidEnterFullscreen();
  simulateBecamePersistentVideo(true);

  EXPECT_EQ(fullscreenElement(), divElement());
  EXPECT_TRUE(videoElement()->isPersistent());
  EXPECT_TRUE(divElement()->containsPersistentVideo());
  EXPECT_TRUE(videoElement()->containsPersistentVideo());

  // The :internal-* rules apply only from the UA stylesheet.
  EXPECT_TRUE(divElement()->matches(":-webkit-full-screen"));
  EXPECT_FALSE(divElement()->matches(":-internal-video-persistent-ancestor"));
  EXPECT_FALSE(videoElement()->matches(":-internal-video-persistent"));
  EXPECT_FALSE(videoElement()->matches(":-internal-video-persistent-ancestor"));
}

TEST_F(HTMLVideoElementPersistentTest, removeContainerWhilePersisting) {
  EXPECT_EQ(fullscreenElement(), nullptr);

  EXPECT_CALL(mockChromeClient(), enterFullscreen(_)).Times(1);
  EXPECT_CALL(mockChromeClient(), exitFullscreen(_)).Times(1);

  UserGestureIndicator gestureIndicator(
      DocumentUserGestureToken::create(&document()));
  Fullscreen::requestFullscreen(*divElement());
  simulateDidEnterFullscreen();
  EXPECT_EQ(fullscreenElement(), divElement());

  simulateBecamePersistentVideo(true);
  Persistent<HTMLDivElement> div = divElement();
  Persistent<HTMLVideoElement> video = videoElement();
  document().body()->removeChild(divElement());

  EXPECT_FALSE(video->isPersistent());
  EXPECT_FALSE(div->containsPersistentVideo());
  EXPECT_FALSE(video->containsPersistentVideo());
}

TEST_F(HTMLVideoElementPersistentTest, removeVideoWhilePersisting) {
  EXPECT_EQ(fullscreenElement(), nullptr);

  EXPECT_CALL(mockChromeClient(), enterFullscreen(_)).Times(1);
  EXPECT_CALL(mockChromeClient(), exitFullscreen(_)).Times(0);

  UserGestureIndicator gestureIndicator(
      DocumentUserGestureToken::create(&document()));
  Fullscreen::requestFullscreen(*divElement());
  simulateDidEnterFullscreen();
  EXPECT_EQ(fullscreenElement(), divElement());

  simulateBecamePersistentVideo(true);
  Persistent<HTMLVideoElement> video = videoElement();
  divElement()->removeChild(videoElement());

  EXPECT_FALSE(video->isPersistent());
  EXPECT_FALSE(divElement()->containsPersistentVideo());
  EXPECT_FALSE(video->containsPersistentVideo());
}

TEST_F(HTMLVideoElementPersistentTest, removeVideoWithLayerWhilePersisting) {
  EXPECT_EQ(fullscreenElement(), nullptr);

  // Inserting a <span> between the <div> and <video>.
  Persistent<Element> span = document().createElement("span");
  divElement()->appendChild(span);
  span->appendChild(videoElement());

  EXPECT_CALL(mockChromeClient(), enterFullscreen(_)).Times(1);
  EXPECT_CALL(mockChromeClient(), exitFullscreen(_)).Times(0);

  UserGestureIndicator gestureIndicator(
      DocumentUserGestureToken::create(&document()));
  Fullscreen::requestFullscreen(*divElement());
  simulateDidEnterFullscreen();
  EXPECT_EQ(fullscreenElement(), divElement());

  simulateBecamePersistentVideo(true);
  Persistent<HTMLVideoElement> video = videoElement();
  span->removeChild(videoElement());

  EXPECT_FALSE(video->isPersistent());
  EXPECT_FALSE(divElement()->containsPersistentVideo());
  EXPECT_FALSE(video->containsPersistentVideo());
  EXPECT_FALSE(span->containsPersistentVideo());
}

TEST_F(HTMLVideoElementPersistentTest, containsPersistentVideoScopedToFS) {
  EXPECT_EQ(fullscreenElement(), nullptr);

  EXPECT_CALL(mockChromeClient(), enterFullscreen(_)).Times(1);
  EXPECT_CALL(mockChromeClient(), exitFullscreen(_)).Times(0);

  UserGestureIndicator gestureIndicator(
      DocumentUserGestureToken::create(&document()));
  Fullscreen::requestFullscreen(*divElement());
  simulateDidEnterFullscreen();
  EXPECT_EQ(fullscreenElement(), divElement());

  simulateBecamePersistentVideo(true);
  EXPECT_FALSE(document().body()->containsPersistentVideo());
}

}  // namespace blink

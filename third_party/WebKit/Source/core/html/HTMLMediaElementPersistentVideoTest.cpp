// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/HTMLMediaElement.h"

#include <memory>

#include "core/dom/DocumentUserGestureToken.h"
#include "core/dom/Fullscreen.h"
#include "core/html/HTMLDivElement.h"
#include "core/loader/EmptyClients.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/UserGestureIndicator.h"
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

class HTMLMediaElementPersistentVideoTest : public ::testing::Test {
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

  HTMLMediaElement* videoElement() {
    return toHTMLMediaElement(document().querySelector("video"));
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

  bool isPersistentVideo() { return videoElement()->m_isPersistentVideo; }

 private:
  std::unique_ptr<DummyPageHolder> m_pageHolder;
  Persistent<MockChromeClient> m_chromeClient;
};

TEST_F(HTMLMediaElementPersistentVideoTest, testWhenNothingIsFullscreen) {
  Sequence s;

  EXPECT_EQ(fullscreenElement(), nullptr);

  EXPECT_CALL(mockChromeClient(), enterFullscreen(_)).InSequence(s);

  // Make the video persistent.
  simulateBecamePersistentVideo(true);
  simulateDidEnterFullscreen();
  EXPECT_EQ(fullscreenElement(), videoElement());
  EXPECT_TRUE(isPersistentVideo());

  // This should be no-op.
  simulateBecamePersistentVideo(true);
  EXPECT_EQ(fullscreenElement(), videoElement());
  EXPECT_TRUE(isPersistentVideo());

  // Make the video not persitent.
  EXPECT_CALL(mockChromeClient(), exitFullscreen(_)).InSequence(s);
  simulateBecamePersistentVideo(false);
  simulateDidExitFullscreen();
  EXPECT_EQ(fullscreenElement(), nullptr);
  EXPECT_FALSE(isPersistentVideo());

  // This should be no-op.
  simulateBecamePersistentVideo(false);
  EXPECT_EQ(fullscreenElement(), nullptr);
  EXPECT_FALSE(isPersistentVideo());
}

TEST_F(HTMLMediaElementPersistentVideoTest, testWhenVideoIsFullscreen) {
  EXPECT_EQ(fullscreenElement(), nullptr);

  EXPECT_CALL(mockChromeClient(), enterFullscreen(_)).Times(1);
  EXPECT_CALL(mockChromeClient(), exitFullscreen(_)).Times(0);

  // Fullscreen the video in advance.
  UserGestureIndicator gestureIndicator(
      DocumentUserGestureToken::create(&document()));
  Fullscreen::requestFullscreen(*videoElement());
  simulateDidEnterFullscreen();
  EXPECT_EQ(fullscreenElement(), videoElement());

  // This should be no-op.
  simulateBecamePersistentVideo(true);
  EXPECT_EQ(fullscreenElement(), videoElement());
  EXPECT_FALSE(isPersistentVideo());

  // This should be no-op.
  simulateBecamePersistentVideo(false);
  EXPECT_EQ(fullscreenElement(), videoElement());
  EXPECT_FALSE(isPersistentVideo());
}

TEST_F(HTMLMediaElementPersistentVideoTest, testWhenDivIsFullscreen) {
  EXPECT_EQ(fullscreenElement(), nullptr);

  EXPECT_CALL(mockChromeClient(), enterFullscreen(_)).Times(2);
  EXPECT_CALL(mockChromeClient(), exitFullscreen(_)).Times(0);

  // Fullscreen the video in advance.
  UserGestureIndicator gestureIndicator(
      DocumentUserGestureToken::create(&document()));
  Fullscreen::requestFullscreen(*divElement());
  simulateDidEnterFullscreen();
  EXPECT_EQ(fullscreenElement(), divElement());

  // Make the video persistent.
  simulateBecamePersistentVideo(true);
  simulateDidEnterFullscreen();
  EXPECT_EQ(fullscreenElement(), videoElement());
  EXPECT_TRUE(isPersistentVideo());

  // This should be no-op.
  simulateBecamePersistentVideo(true);
  EXPECT_EQ(fullscreenElement(), videoElement());
  EXPECT_TRUE(isPersistentVideo());

  // Make the video not persistent. Don't call simulateDidExitFullscreen as
  // there still a fullscreen element in stack.
  simulateBecamePersistentVideo(false);
  EXPECT_EQ(fullscreenElement(), divElement());
  EXPECT_FALSE(isPersistentVideo());
}

}  // namespace blink

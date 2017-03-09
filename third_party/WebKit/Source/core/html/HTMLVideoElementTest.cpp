// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/HTMLVideoElement.h"

#include <memory>
#include "core/dom/Document.h"
#include "core/dom/DocumentUserGestureToken.h"
#include "core/loader/EmptyClients.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/UserGestureIndicator.h"
#include "platform/network/NetworkStateNotifier.h"
#include "platform/testing/EmptyWebMediaPlayer.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/WebSize.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/PtrUtil.h"

namespace blink {

namespace {

class MockWebMediaPlayer : public EmptyWebMediaPlayer {
 public:
  MOCK_METHOD1(setBufferingStrategy, void(BufferingStrategy));
};

class StubLocalFrameClient : public EmptyLocalFrameClient {
 public:
  static StubLocalFrameClient* create() { return new StubLocalFrameClient; }

  std::unique_ptr<WebMediaPlayer> createWebMediaPlayer(
      HTMLMediaElement&,
      const WebMediaPlayerSource&,
      WebMediaPlayerClient*) override {
    return WTF::wrapUnique(new MockWebMediaPlayer);
  }
};

}  // namespace

class HTMLVideoElementTest : public ::testing::Test {
 protected:
  HTMLVideoElementTest()
      : m_dummyPageHolder(
            DummyPageHolder::create(IntSize(640, 360),
                                    nullptr,
                                    StubLocalFrameClient::create())) {
    // TODO(sandersd): This should be done by a settings initializer.
    networkStateNotifier().setWebConnection(WebConnectionTypeWifi, 54.0);
    m_video = HTMLVideoElement::create(m_dummyPageHolder->document());
  }

  void setSrc(const AtomicString& url) {
    m_video->setSrc(url);
    testing::runPendingTasks();
  }

  MockWebMediaPlayer* webMediaPlayer() {
    return static_cast<MockWebMediaPlayer*>(m_video->webMediaPlayer());
  }

  std::unique_ptr<DummyPageHolder> m_dummyPageHolder;
  Persistent<HTMLVideoElement> m_video;
};

TEST_F(HTMLVideoElementTest, setBufferingStrategy_NonUserPause) {
  setSrc("http://foo.bar/");
  MockWebMediaPlayer* player = webMediaPlayer();
  ASSERT_TRUE(player);

  // On play, the strategy is set to normal.
  EXPECT_CALL(*player,
              setBufferingStrategy(WebMediaPlayer::BufferingStrategy::Normal));
  m_video->play();
  ::testing::Mock::VerifyAndClearExpectations(player);

  // On a non-user pause, the strategy is not changed.
  m_video->pause();
  ::testing::Mock::VerifyAndClearExpectations(player);

  // On play, the strategy is set to normal.
  EXPECT_CALL(*player,
              setBufferingStrategy(WebMediaPlayer::BufferingStrategy::Normal));
  m_video->play();
  ::testing::Mock::VerifyAndClearExpectations(player);
}

TEST_F(HTMLVideoElementTest, setBufferingStrategy_UserPause) {
  setSrc("http://foo.bar/");
  MockWebMediaPlayer* player = webMediaPlayer();
  ASSERT_TRUE(player);

  // On play, the strategy is set to normal.
  EXPECT_CALL(*player,
              setBufferingStrategy(WebMediaPlayer::BufferingStrategy::Normal));
  m_video->play();
  ::testing::Mock::VerifyAndClearExpectations(player);

  // On a user pause, the strategy is set to aggressive.
  EXPECT_CALL(*player, setBufferingStrategy(
                           WebMediaPlayer::BufferingStrategy::Aggressive));
  {
    UserGestureIndicator gesture(
        DocumentUserGestureToken::create(&m_video->document()));
    m_video->pause();
  }
  ::testing::Mock::VerifyAndClearExpectations(player);

  // On play, the strategy is set to normal.
  EXPECT_CALL(*player,
              setBufferingStrategy(WebMediaPlayer::BufferingStrategy::Normal));
  m_video->play();
  ::testing::Mock::VerifyAndClearExpectations(player);
}

}  // namespace blink

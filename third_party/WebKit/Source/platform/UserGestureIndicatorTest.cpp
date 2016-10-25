// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/UserGestureIndicator.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class TestUserGestureToken final : public UserGestureToken {
  WTF_MAKE_NONCOPYABLE(TestUserGestureToken);

 public:
  static PassRefPtr<UserGestureToken> create(
      Status status = PossiblyExistingGesture) {
    return adoptRef(new TestUserGestureToken(status));
  }

  ~TestUserGestureToken() final {}

 private:
  TestUserGestureToken(Status status) : UserGestureToken(status) {}
};

// Checks for the initial state of UserGestureIndicator.
TEST(UserGestureIndicatorTest, InitialState) {
  EXPECT_FALSE(UserGestureIndicator::utilizeUserGesture());
  EXPECT_EQ(nullptr, UserGestureIndicator::currentToken());
  EXPECT_FALSE(UserGestureIndicator::consumeUserGesture());
}

TEST(UserGestureIndicatorTest, ConstructedWithNewUserGesture) {
  UserGestureIndicator userGestureScope(
      TestUserGestureToken::create(UserGestureToken::NewGesture));

  EXPECT_TRUE(UserGestureIndicator::utilizeUserGesture());
  EXPECT_NE(nullptr, UserGestureIndicator::currentToken());

  EXPECT_TRUE(UserGestureIndicator::consumeUserGesture());
}

TEST(UserGestureIndicatorTest, ConstructedWithUserGesture) {
  UserGestureIndicator userGestureScope(TestUserGestureToken::create());

  EXPECT_TRUE(UserGestureIndicator::utilizeUserGesture());
  EXPECT_NE(nullptr, UserGestureIndicator::currentToken());

  EXPECT_TRUE(UserGestureIndicator::consumeUserGesture());
}

TEST(UserGestureIndicatorTest, ConstructedWithNoUserGesture) {
  UserGestureIndicator userGestureScope(nullptr);

  EXPECT_FALSE(UserGestureIndicator::utilizeUserGesture());
  EXPECT_EQ(nullptr, UserGestureIndicator::currentToken());

  EXPECT_FALSE(UserGestureIndicator::consumeUserGesture());
}

// Check that after UserGestureIndicator destruction state will be cleared.
TEST(UserGestureIndicatorTest, DestructUserGestureIndicator) {
  {
    UserGestureIndicator userGestureScope(TestUserGestureToken::create());

    EXPECT_TRUE(UserGestureIndicator::utilizeUserGesture());
    EXPECT_NE(nullptr, UserGestureIndicator::currentToken());
  }

  EXPECT_FALSE(UserGestureIndicator::utilizeUserGesture());
  EXPECT_EQ(nullptr, UserGestureIndicator::currentToken());
  EXPECT_FALSE(UserGestureIndicator::consumeUserGesture());
}

// Tests creation of scoped UserGestureIndicator objects.
TEST(UserGestureIndicatorTest, ScopedNewUserGestureIndicators) {
  // Root GestureIndicator and GestureToken.
  UserGestureIndicator userGestureScope(
      TestUserGestureToken::create(UserGestureToken::NewGesture));

  EXPECT_TRUE(UserGestureIndicator::utilizeUserGesture());
  EXPECT_NE(nullptr, UserGestureIndicator::currentToken());
  {
    // Construct inner UserGestureIndicator.
    // It should share GestureToken with the root indicator.
    UserGestureIndicator innerUserGesture(
        TestUserGestureToken::create(UserGestureToken::NewGesture));

    EXPECT_TRUE(UserGestureIndicator::utilizeUserGesture());
    EXPECT_NE(nullptr, UserGestureIndicator::currentToken());

    // Consume inner gesture.
    EXPECT_TRUE(UserGestureIndicator::consumeUserGesture());
  }

  EXPECT_TRUE(UserGestureIndicator::utilizeUserGesture());
  EXPECT_NE(nullptr, UserGestureIndicator::currentToken());

  // Consume root gesture.
  EXPECT_TRUE(UserGestureIndicator::consumeUserGesture());
  EXPECT_FALSE(UserGestureIndicator::utilizeUserGesture());
  EXPECT_NE(nullptr, UserGestureIndicator::currentToken());
}

class UsedCallback : public UserGestureUtilizedCallback {
 public:
  UsedCallback() : m_usedCount(0) {}

  void userGestureUtilized() override { m_usedCount++; }

  unsigned getAndResetUsedCount() {
    unsigned curCount = m_usedCount;
    m_usedCount = 0;
    return curCount;
  }

 private:
  unsigned m_usedCount;
};

// Tests callback invocation.
TEST(UserGestureIndicatorTest, Callback) {
  UsedCallback cb;

  {
    UserGestureIndicator userGestureScope(TestUserGestureToken::create());
    UserGestureIndicator::currentToken()->setUserGestureUtilizedCallback(&cb);
    EXPECT_EQ(0u, cb.getAndResetUsedCount());

    // Untracked doesn't invoke the callback
    EXPECT_TRUE(UserGestureIndicator::processingUserGesture());
    EXPECT_EQ(0u, cb.getAndResetUsedCount());

    // But processingUserGesture does
    EXPECT_TRUE(UserGestureIndicator::utilizeUserGesture());
    EXPECT_EQ(1u, cb.getAndResetUsedCount());

    // But only the first time
    EXPECT_TRUE(UserGestureIndicator::utilizeUserGesture());
    EXPECT_TRUE(UserGestureIndicator::consumeUserGesture());
    EXPECT_EQ(0u, cb.getAndResetUsedCount());
  }
  EXPECT_EQ(0u, cb.getAndResetUsedCount());

  {
    UserGestureIndicator userGestureScope(TestUserGestureToken::create());
    UserGestureIndicator::currentToken()->setUserGestureUtilizedCallback(&cb);

    // Consume also invokes the callback
    EXPECT_TRUE(UserGestureIndicator::consumeUserGesture());
    EXPECT_EQ(1u, cb.getAndResetUsedCount());

    // But only once
    EXPECT_FALSE(UserGestureIndicator::utilizeUserGesture());
    EXPECT_FALSE(UserGestureIndicator::consumeUserGesture());
    EXPECT_EQ(0u, cb.getAndResetUsedCount());
  }

  {
    std::unique_ptr<UserGestureIndicator> userGestureScope(
        new UserGestureIndicator(TestUserGestureToken::create()));
    RefPtr<UserGestureToken> token = UserGestureIndicator::currentToken();
    token->setUserGestureUtilizedCallback(&cb);
    userGestureScope.reset();

    // The callback should be cleared when the UseGestureIndicator is deleted.
    EXPECT_FALSE(UserGestureIndicator::utilizeUserGesture());
    EXPECT_EQ(0u, cb.getAndResetUsedCount());
  }

  // The callback isn't invoked outside the scope of the UGI
  EXPECT_FALSE(UserGestureIndicator::utilizeUserGesture());
  EXPECT_EQ(0u, cb.getAndResetUsedCount());
  EXPECT_FALSE(UserGestureIndicator::consumeUserGesture());
  EXPECT_EQ(0u, cb.getAndResetUsedCount());
}

}  // namespace blink

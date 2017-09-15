// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/UserGestureIndicator.h"

#include "platform/wtf/CurrentTime.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

static double g_current_time = 1000.0;

static void AdvanceClock(double seconds) {
  g_current_time += seconds;
}

static double MockTimeFunction() {
  return g_current_time;
}

class TestUserGestureToken final : public UserGestureToken {
  WTF_MAKE_NONCOPYABLE(TestUserGestureToken);

 public:
  static RefPtr<UserGestureToken> Create(
      Status status = kPossiblyExistingGesture) {
    return AdoptRef(new TestUserGestureToken(status));
  }

 private:
  TestUserGestureToken(Status status) : UserGestureToken(status) {}
};

// Checks for the initial state of UserGestureIndicator.
TEST(UserGestureIndicatorTest, InitialState) {
  EXPECT_FALSE(UserGestureIndicator::ProcessingUserGesture());
  EXPECT_EQ(nullptr, UserGestureIndicator::CurrentToken());
  EXPECT_FALSE(UserGestureIndicator::ConsumeUserGesture());
}

TEST(UserGestureIndicatorTest, ConstructedWithNewUserGesture) {
  UserGestureIndicator user_gesture_scope(
      TestUserGestureToken::Create(UserGestureToken::kNewGesture));

  EXPECT_TRUE(UserGestureIndicator::ProcessingUserGesture());
  EXPECT_NE(nullptr, UserGestureIndicator::CurrentToken());

  EXPECT_TRUE(UserGestureIndicator::ConsumeUserGesture());
}

TEST(UserGestureIndicatorTest, ConstructedWithUserGesture) {
  UserGestureIndicator user_gesture_scope(TestUserGestureToken::Create());

  EXPECT_TRUE(UserGestureIndicator::ProcessingUserGesture());
  EXPECT_NE(nullptr, UserGestureIndicator::CurrentToken());

  EXPECT_TRUE(UserGestureIndicator::ConsumeUserGesture());
}

TEST(UserGestureIndicatorTest, ConstructedWithNoUserGesture) {
  UserGestureIndicator user_gesture_scope(nullptr);

  EXPECT_FALSE(UserGestureIndicator::ProcessingUserGesture());
  EXPECT_EQ(nullptr, UserGestureIndicator::CurrentToken());

  EXPECT_FALSE(UserGestureIndicator::ConsumeUserGesture());
}

// Check that after UserGestureIndicator destruction state will be cleared.
TEST(UserGestureIndicatorTest, DestructUserGestureIndicator) {
  {
    UserGestureIndicator user_gesture_scope(TestUserGestureToken::Create());

    EXPECT_TRUE(UserGestureIndicator::ProcessingUserGesture());
    EXPECT_NE(nullptr, UserGestureIndicator::CurrentToken());
  }

  EXPECT_FALSE(UserGestureIndicator::ProcessingUserGesture());
  EXPECT_EQ(nullptr, UserGestureIndicator::CurrentToken());
  EXPECT_FALSE(UserGestureIndicator::ConsumeUserGesture());
}

// Tests creation of scoped UserGestureIndicator objects.
TEST(UserGestureIndicatorTest, ScopedNewUserGestureIndicators) {
  // Root GestureIndicator and GestureToken.
  UserGestureIndicator user_gesture_scope(
      TestUserGestureToken::Create(UserGestureToken::kNewGesture));

  EXPECT_TRUE(UserGestureIndicator::ProcessingUserGesture());
  EXPECT_NE(nullptr, UserGestureIndicator::CurrentToken());
  {
    // Construct inner UserGestureIndicator.
    // It should share GestureToken with the root indicator.
    UserGestureIndicator inner_user_gesture(
        TestUserGestureToken::Create(UserGestureToken::kNewGesture));

    EXPECT_TRUE(UserGestureIndicator::ProcessingUserGesture());
    EXPECT_NE(nullptr, UserGestureIndicator::CurrentToken());

    // Consume inner gesture.
    EXPECT_TRUE(UserGestureIndicator::ConsumeUserGesture());
  }

  EXPECT_TRUE(UserGestureIndicator::ProcessingUserGesture());
  EXPECT_NE(nullptr, UserGestureIndicator::CurrentToken());

  // Consume root gesture.
  EXPECT_TRUE(UserGestureIndicator::ConsumeUserGesture());
  EXPECT_FALSE(UserGestureIndicator::ProcessingUserGesture());
  EXPECT_NE(nullptr, UserGestureIndicator::CurrentToken());
}

TEST(UserGestureIndicatorTest, MultipleGesturesWithTheSameToken) {
  UserGestureIndicator indicator(
      TestUserGestureToken::Create(UserGestureToken::kNewGesture));
  EXPECT_TRUE(UserGestureIndicator::ProcessingUserGesture());
  EXPECT_NE(nullptr, UserGestureIndicator::CurrentToken());
  {
    // Construct an inner indicator that shares the same token.
    UserGestureIndicator inner_indicator(UserGestureIndicator::CurrentToken());
    EXPECT_TRUE(UserGestureIndicator::ProcessingUserGesture());
    EXPECT_NE(nullptr, UserGestureIndicator::CurrentToken());
  }
  // Though the inner indicator was destroyed, the outer is still present (and
  // the gesture hasn't been consumed), so it should still be processing a user
  // gesture.
  EXPECT_TRUE(UserGestureIndicator::ProcessingUserGesture());
  EXPECT_NE(nullptr, UserGestureIndicator::CurrentToken());
}

TEST(UserGestureIndicatorTest, Timeouts) {
  TimeFunction previous = SetTimeFunctionsForTesting(MockTimeFunction);

  {
    // Token times out after 1 second.
    RefPtr<UserGestureToken> token = TestUserGestureToken::Create();
    EXPECT_TRUE(token->HasGestures());
    UserGestureIndicator user_gesture_scope(token.Get());
    EXPECT_TRUE(token->HasGestures());
    AdvanceClock(0.75);
    EXPECT_TRUE(token->HasGestures());
    AdvanceClock(0.75);
    EXPECT_FALSE(token->HasGestures());
  }

  {
    // Timestamp is reset when a token is put in a UserGestureIndicator.
    RefPtr<UserGestureToken> token = TestUserGestureToken::Create();
    EXPECT_TRUE(token->HasGestures());
    AdvanceClock(0.75);
    EXPECT_TRUE(token->HasGestures());
    UserGestureIndicator user_gesture_scope(token.Get());
    AdvanceClock(0.75);
    EXPECT_TRUE(token->HasGestures());
    AdvanceClock(0.75);
    EXPECT_FALSE(token->HasGestures());
  }

  SetTimeFunctionsForTesting(previous);
}

}  // namespace blink

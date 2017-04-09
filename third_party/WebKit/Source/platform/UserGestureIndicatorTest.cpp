// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/UserGestureIndicator.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/CurrentTime.h"

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
  static PassRefPtr<UserGestureToken> Create(
      Status status = kPossiblyExistingGesture) {
    return AdoptRef(new TestUserGestureToken(status));
  }

 private:
  TestUserGestureToken(Status status) : UserGestureToken(status) {}
};

// Checks for the initial state of UserGestureIndicator.
TEST(UserGestureIndicatorTest, InitialState) {
  EXPECT_FALSE(UserGestureIndicator::UtilizeUserGesture());
  EXPECT_EQ(nullptr, UserGestureIndicator::CurrentToken());
  EXPECT_FALSE(UserGestureIndicator::ConsumeUserGesture());
}

TEST(UserGestureIndicatorTest, ConstructedWithNewUserGesture) {
  UserGestureIndicator user_gesture_scope(
      TestUserGestureToken::Create(UserGestureToken::kNewGesture));

  EXPECT_TRUE(UserGestureIndicator::UtilizeUserGesture());
  EXPECT_NE(nullptr, UserGestureIndicator::CurrentToken());

  EXPECT_TRUE(UserGestureIndicator::ConsumeUserGesture());
}

TEST(UserGestureIndicatorTest, ConstructedWithUserGesture) {
  UserGestureIndicator user_gesture_scope(TestUserGestureToken::Create());

  EXPECT_TRUE(UserGestureIndicator::UtilizeUserGesture());
  EXPECT_NE(nullptr, UserGestureIndicator::CurrentToken());

  EXPECT_TRUE(UserGestureIndicator::ConsumeUserGesture());
}

TEST(UserGestureIndicatorTest, ConstructedWithNoUserGesture) {
  UserGestureIndicator user_gesture_scope(nullptr);

  EXPECT_FALSE(UserGestureIndicator::UtilizeUserGesture());
  EXPECT_EQ(nullptr, UserGestureIndicator::CurrentToken());

  EXPECT_FALSE(UserGestureIndicator::ConsumeUserGesture());
}

// Check that after UserGestureIndicator destruction state will be cleared.
TEST(UserGestureIndicatorTest, DestructUserGestureIndicator) {
  {
    UserGestureIndicator user_gesture_scope(TestUserGestureToken::Create());

    EXPECT_TRUE(UserGestureIndicator::UtilizeUserGesture());
    EXPECT_NE(nullptr, UserGestureIndicator::CurrentToken());
  }

  EXPECT_FALSE(UserGestureIndicator::UtilizeUserGesture());
  EXPECT_EQ(nullptr, UserGestureIndicator::CurrentToken());
  EXPECT_FALSE(UserGestureIndicator::ConsumeUserGesture());
}

// Tests creation of scoped UserGestureIndicator objects.
TEST(UserGestureIndicatorTest, ScopedNewUserGestureIndicators) {
  // Root GestureIndicator and GestureToken.
  UserGestureIndicator user_gesture_scope(
      TestUserGestureToken::Create(UserGestureToken::kNewGesture));

  EXPECT_TRUE(UserGestureIndicator::UtilizeUserGesture());
  EXPECT_NE(nullptr, UserGestureIndicator::CurrentToken());
  {
    // Construct inner UserGestureIndicator.
    // It should share GestureToken with the root indicator.
    UserGestureIndicator inner_user_gesture(
        TestUserGestureToken::Create(UserGestureToken::kNewGesture));

    EXPECT_TRUE(UserGestureIndicator::UtilizeUserGesture());
    EXPECT_NE(nullptr, UserGestureIndicator::CurrentToken());

    // Consume inner gesture.
    EXPECT_TRUE(UserGestureIndicator::ConsumeUserGesture());
  }

  EXPECT_TRUE(UserGestureIndicator::UtilizeUserGesture());
  EXPECT_NE(nullptr, UserGestureIndicator::CurrentToken());

  // Consume root gesture.
  EXPECT_TRUE(UserGestureIndicator::ConsumeUserGesture());
  EXPECT_FALSE(UserGestureIndicator::UtilizeUserGesture());
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

class UsedCallback : public UserGestureUtilizedCallback {
 public:
  UsedCallback() : used_count_(0) {}

  void UserGestureUtilized() override { used_count_++; }

  unsigned GetAndResetUsedCount() {
    unsigned cur_count = used_count_;
    used_count_ = 0;
    return cur_count;
  }

 private:
  unsigned used_count_;
};

// Tests callback invocation.
TEST(UserGestureIndicatorTest, Callback) {
  UsedCallback cb;

  {
    UserGestureIndicator user_gesture_scope(TestUserGestureToken::Create());
    UserGestureIndicator::CurrentToken()->SetUserGestureUtilizedCallback(&cb);
    EXPECT_EQ(0u, cb.GetAndResetUsedCount());

    // Untracked doesn't invoke the callback
    EXPECT_TRUE(UserGestureIndicator::ProcessingUserGesture());
    EXPECT_EQ(0u, cb.GetAndResetUsedCount());

    // But processingUserGesture does
    EXPECT_TRUE(UserGestureIndicator::UtilizeUserGesture());
    EXPECT_EQ(1u, cb.GetAndResetUsedCount());

    // But only the first time
    EXPECT_TRUE(UserGestureIndicator::UtilizeUserGesture());
    EXPECT_TRUE(UserGestureIndicator::ConsumeUserGesture());
    EXPECT_EQ(0u, cb.GetAndResetUsedCount());
  }
  EXPECT_EQ(0u, cb.GetAndResetUsedCount());

  {
    UserGestureIndicator user_gesture_scope(TestUserGestureToken::Create());
    UserGestureIndicator::CurrentToken()->SetUserGestureUtilizedCallback(&cb);

    // Consume also invokes the callback
    EXPECT_TRUE(UserGestureIndicator::ConsumeUserGesture());
    EXPECT_EQ(1u, cb.GetAndResetUsedCount());

    // But only once
    EXPECT_FALSE(UserGestureIndicator::UtilizeUserGesture());
    EXPECT_FALSE(UserGestureIndicator::ConsumeUserGesture());
    EXPECT_EQ(0u, cb.GetAndResetUsedCount());
  }

  {
    std::unique_ptr<UserGestureIndicator> user_gesture_scope(
        new UserGestureIndicator(TestUserGestureToken::Create()));
    RefPtr<UserGestureToken> token = UserGestureIndicator::CurrentToken();
    token->SetUserGestureUtilizedCallback(&cb);
    user_gesture_scope.reset();

    // The callback should be cleared when the UseGestureIndicator is deleted.
    EXPECT_FALSE(UserGestureIndicator::UtilizeUserGesture());
    EXPECT_EQ(0u, cb.GetAndResetUsedCount());
  }

  // The callback isn't invoked outside the scope of the UGI
  EXPECT_FALSE(UserGestureIndicator::UtilizeUserGesture());
  EXPECT_EQ(0u, cb.GetAndResetUsedCount());
  EXPECT_FALSE(UserGestureIndicator::ConsumeUserGesture());
  EXPECT_EQ(0u, cb.GetAndResetUsedCount());
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

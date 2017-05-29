// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/Frame.h"

#include "core/dom/UserGestureIndicator.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class FrameTest : public ::testing::Test {
 public:
  void SetUp() override {
    dummy_page_holder_ = DummyPageHolder::Create(IntSize(800, 600));
    ASSERT_FALSE(GetDocument().GetFrame()->HasReceivedUserGesture());
  }

  Document& GetDocument() const { return dummy_page_holder_->GetDocument(); }

 private:
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
};

TEST_F(FrameTest, NoGesture) {
  // A nullptr Document* will not set user gesture state.
  UserGestureToken::Create(nullptr);
  EXPECT_FALSE(GetDocument().GetFrame()->HasReceivedUserGesture());
}

TEST_F(FrameTest, PossiblyExisting) {
  // A non-null Document* will set state, but a subsequent nullptr Document*
  // token will not override it.
  UserGestureToken::Create(&GetDocument());
  EXPECT_TRUE(GetDocument().GetFrame()->HasReceivedUserGesture());
  UserGestureToken::Create(nullptr);
  EXPECT_TRUE(GetDocument().GetFrame()->HasReceivedUserGesture());
}

TEST_F(FrameTest, NewGesture) {
  // UserGestureToken::Status doesn't impact Document gesture state.
  UserGestureToken::Create(&GetDocument(), UserGestureToken::kNewGesture);
  EXPECT_TRUE(GetDocument().GetFrame()->HasReceivedUserGesture());
}

TEST_F(FrameTest, Navigate) {
  UserGestureToken::Create(&GetDocument());
  ASSERT_TRUE(GetDocument().GetFrame()->HasReceivedUserGesture());

  // Navigate to a different Document. In the main frame, user gesture state
  // will get reset.
  GetDocument().GetFrame()->Loader().Load(
      FrameLoadRequest(nullptr, ResourceRequest()));
  EXPECT_FALSE(GetDocument().GetFrame()->HasReceivedUserGesture());
}

}  // namespace blink

// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/DocumentUserGestureToken.h"

#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class DocumentUserGestureTokenTest : public ::testing::Test {
 public:
  void SetUp() override {
    dummy_page_holder_ = DummyPageHolder::Create(IntSize(800, 600));
    ASSERT_FALSE(GetDocument().GetFrame()->HasReceivedUserGesture());
  }

  Document& GetDocument() const { return dummy_page_holder_->GetDocument(); }

 private:
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
};

TEST_F(DocumentUserGestureTokenTest, NoGesture) {
  // A nullptr Document* will not set user gesture state.
  DocumentUserGestureToken::Create(nullptr);
  EXPECT_FALSE(GetDocument().GetFrame()->HasReceivedUserGesture());
}

TEST_F(DocumentUserGestureTokenTest, PossiblyExisting) {
  // A non-null Document* will set state, but a subsequent nullptr Document*
  // token will not override it.
  DocumentUserGestureToken::Create(&GetDocument());
  EXPECT_TRUE(GetDocument().GetFrame()->HasReceivedUserGesture());
  DocumentUserGestureToken::Create(nullptr);
  EXPECT_TRUE(GetDocument().GetFrame()->HasReceivedUserGesture());
}

TEST_F(DocumentUserGestureTokenTest, NewGesture) {
  // UserGestureToken::Status doesn't impact Document gesture state.
  DocumentUserGestureToken::Create(&GetDocument(),
                                   UserGestureToken::kNewGesture);
  EXPECT_TRUE(GetDocument().GetFrame()->HasReceivedUserGesture());
}

TEST_F(DocumentUserGestureTokenTest, Navigate) {
  DocumentUserGestureToken::Create(&GetDocument());
  ASSERT_TRUE(GetDocument().GetFrame()->HasReceivedUserGesture());

  // Navigate to a different Document. In the main frame, user gesture state
  // will get reset.
  GetDocument().GetFrame()->Loader().Load(
      FrameLoadRequest(nullptr, ResourceRequest()));
  EXPECT_FALSE(GetDocument().GetFrame()->HasReceivedUserGesture());
}

}  // namespace blink

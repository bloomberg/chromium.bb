// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/DocumentUserGestureToken.h"

#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(DocumentUserGestureTokenTest, DocumentUserGestureState) {
  std::unique_ptr<DummyPageHolder> dummyPageHolder =
      DummyPageHolder::create(IntSize(800, 600));
  Document& document = dummyPageHolder->document();
  ASSERT_FALSE(document.hasReceivedUserGesture());

  // A nullptr Document* will not set user gesture state.
  DocumentUserGestureToken::create(nullptr);
  EXPECT_FALSE(document.hasReceivedUserGesture());

  // A non-null Document* will set state, but a subsequent nullptr Document*
  // token will not override it.
  DocumentUserGestureToken::create(&document);
  EXPECT_TRUE(document.hasReceivedUserGesture());
  DocumentUserGestureToken::create(nullptr);
  EXPECT_TRUE(document.hasReceivedUserGesture());
  document.clearHasReceivedUserGesture();
  ASSERT_FALSE(document.hasReceivedUserGesture());

  // UserGestureToken::Status doesn't impact Document gesture state.
  DocumentUserGestureToken::create(&document, UserGestureToken::NewGesture);
  EXPECT_TRUE(document.hasReceivedUserGesture());
}

}  // namespace blink

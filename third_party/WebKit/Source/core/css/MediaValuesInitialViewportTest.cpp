// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/MediaValuesInitialViewport.h"

#include "core/frame/LocalFrameView.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class MediaValuesInitialViewportTest : public ::testing::Test {
 protected:
  Document& GetDocument() const { return dummy_page_holder_->GetDocument(); }

 private:
  void SetUp() override {
    dummy_page_holder_ = DummyPageHolder::Create(IntSize(320, 480));
    GetDocument().View()->SetInitialViewportSize(IntSize(320, 480));
  }

  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
};

TEST_F(MediaValuesInitialViewportTest, InitialViewportSize) {
  LocalFrameView* view = GetDocument().View();
  ASSERT_TRUE(view);
  EXPECT_TRUE(view->LayoutSizeFixedToFrameSize());

  MediaValues* media_values =
      MediaValuesInitialViewport::Create(*GetDocument().GetFrame());
  EXPECT_EQ(320, media_values->ViewportWidth());
  EXPECT_EQ(480, media_values->ViewportHeight());

  view->SetLayoutSizeFixedToFrameSize(false);
  view->SetLayoutSize(IntSize(800, 600));
  EXPECT_EQ(320, media_values->ViewportWidth());
  EXPECT_EQ(480, media_values->ViewportHeight());
}

}  // namespace blink

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/bounded_scroll_view.h"

#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/size.h"
#include "ui/views/test/test_views.h"

namespace message_center {

namespace test {

const int kMinHeight = 50;
const int kMaxHeight = 100;

const int kWidth = 100;

class BoundedScrollViewTest : public testing::Test {
 public:
  BoundedScrollViewTest() {}
  virtual ~BoundedScrollViewTest() {}

  virtual void SetUp() OVERRIDE {
    scroller_.reset(
        new message_center::BoundedScrollView(kMinHeight, kMaxHeight));
  }

  virtual void TearDown() OVERRIDE { scroller_.reset(); }

 protected:
  message_center::BoundedScrollView* scroller() { return scroller_.get(); }

 private:
  scoped_ptr<message_center::BoundedScrollView> scroller_;
};

TEST_F(BoundedScrollViewTest, NormalSizeContentTest) {
  const int kNormalContentHeight = 75;
  scroller()->SetContents(
      new views::StaticSizedView(gfx::Size(kWidth, kNormalContentHeight)));

  EXPECT_EQ(gfx::Size(kWidth, kNormalContentHeight),
            scroller()->GetPreferredSize());

  scroller()->SizeToPreferredSize();
  scroller()->Layout();

  EXPECT_EQ(gfx::Size(kWidth, kNormalContentHeight),
            scroller()->contents()->size());
  EXPECT_EQ(gfx::Size(kWidth, kNormalContentHeight), scroller()->size());
}

TEST_F(BoundedScrollViewTest, ShortContentTest) {
  const int kShortContentHeight = 10;
  scroller()->SetContents(
      new views::StaticSizedView(gfx::Size(kWidth, kShortContentHeight)));

  EXPECT_EQ(gfx::Size(kWidth, kMinHeight), scroller()->GetPreferredSize());

  scroller()->SizeToPreferredSize();
  scroller()->Layout();

  EXPECT_EQ(gfx::Size(kWidth, kShortContentHeight),
            scroller()->contents()->size());
  EXPECT_EQ(gfx::Size(kWidth, kMinHeight), scroller()->size());
}

TEST_F(BoundedScrollViewTest, TallContentTest) {
  const int kTallContentHeight = 1000;
  scroller()->SetContents(
      new views::StaticSizedView(gfx::Size(kWidth, kTallContentHeight)));

  EXPECT_EQ(gfx::Size(kWidth, kMaxHeight), scroller()->GetPreferredSize());

  scroller()->SizeToPreferredSize();
  scroller()->Layout();

  EXPECT_EQ(gfx::Size(kWidth, kTallContentHeight),
            scroller()->contents()->size());
  EXPECT_EQ(gfx::Size(kWidth, kMaxHeight), scroller()->size());
}

TEST_F(BoundedScrollViewTest, NeedsScrollBarTest) {
  // Create a view that will be much taller than it is high.
  scroller()->SetContents(new views::ProportionallySizedView(1000));

  // Without any width, it will default to 0,0 but be overridden by min height.
  scroller()->SizeToPreferredSize();
  EXPECT_EQ(gfx::Size(0, kMinHeight), scroller()->GetPreferredSize());

  gfx::Size new_size(kWidth, scroller()->GetHeightForWidth(kWidth));
  scroller()->SetSize(new_size);
  scroller()->Layout();

  int scroll_bar_width = scroller()->GetScrollBarWidth();
  int expected_width = kWidth - scroll_bar_width;
  EXPECT_EQ(scroller()->contents()->size().width(), expected_width);
  EXPECT_EQ(scroller()->contents()->size().height(), 1000 * expected_width);
  EXPECT_EQ(gfx::Size(kWidth, kMaxHeight), scroller()->size());
}

}  // namespace test

}  // namespace message_center

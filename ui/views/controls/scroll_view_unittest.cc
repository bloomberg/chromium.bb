// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/scroll_view.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace views {

namespace {

class CustomView : public View {
 public:
  CustomView() {}

  void SetPreferredSize(const gfx::Size& size) {
    preferred_size_ = size;
    PreferredSizeChanged();
  }

  virtual gfx::Size GetPreferredSize() OVERRIDE {
    return preferred_size_;
  }

 private:
  gfx::Size preferred_size_;

  DISALLOW_COPY_AND_ASSIGN(CustomView);
};

}  // namespace

// Verifies the viewport is sized to fit the available space.
TEST(ScrollViewTest, ViewportSizedToFit) {
  ScrollView scroll_view;
  View* contents = new View;
  scroll_view.SetContents(contents);
  scroll_view.SetBoundsRect(gfx::Rect(0, 0, 100, 100));
  scroll_view.Layout();
  EXPECT_EQ("0,0 100x100", contents->parent()->bounds().ToString());
}

// Verifies the scrollbars are added as necessary.
TEST(ScrollViewTest, ScrollBars) {
  ScrollView scroll_view;
  CustomView* contents = new CustomView;
  scroll_view.SetContents(contents);
  scroll_view.SetBoundsRect(gfx::Rect(0, 0, 100, 100));

  // Size the contents such that vertical scrollbar is needed.
  contents->SetBounds(0, 0, 50, 400);
  scroll_view.Layout();
  EXPECT_EQ(100 - scroll_view.GetScrollBarWidth(), contents->parent()->width());
  EXPECT_EQ(100, contents->parent()->height());
  EXPECT_TRUE(!scroll_view.horizontal_scroll_bar() ||
              !scroll_view.horizontal_scroll_bar()->visible());
  ASSERT_TRUE(scroll_view.vertical_scroll_bar() != NULL);
  EXPECT_TRUE(scroll_view.vertical_scroll_bar()->visible());

  // Size the contents such that horizontal scrollbar is needed.
  contents->SetBounds(0, 0, 400, 50);
  scroll_view.Layout();
  EXPECT_EQ(100, contents->parent()->width());
  EXPECT_EQ(100 - scroll_view.GetScrollBarHeight(),
            contents->parent()->height());
  ASSERT_TRUE(scroll_view.horizontal_scroll_bar() != NULL);
  EXPECT_TRUE(scroll_view.horizontal_scroll_bar()->visible());
  EXPECT_TRUE(!scroll_view.vertical_scroll_bar() ||
              !scroll_view.vertical_scroll_bar()->visible());

  // Both horizontal and vertical.
  contents->SetBounds(0, 0, 300, 400);
  scroll_view.Layout();
  EXPECT_EQ(100 - scroll_view.GetScrollBarWidth(), contents->parent()->width());
  EXPECT_EQ(100 - scroll_view.GetScrollBarHeight(),
            contents->parent()->height());
  ASSERT_TRUE(scroll_view.horizontal_scroll_bar() != NULL);
  EXPECT_TRUE(scroll_view.horizontal_scroll_bar()->visible());
  ASSERT_TRUE(scroll_view.vertical_scroll_bar() != NULL);
  EXPECT_TRUE(scroll_view.vertical_scroll_bar()->visible());
}

}  // namespace views

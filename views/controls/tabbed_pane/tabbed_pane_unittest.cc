// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "views/controls/tabbed_pane/tabbed_pane.h"
#include "views/window/window.h"
#include "views/window/window_delegate.h"

namespace views {

// A view for testing that takes a fixed preferred size upon construction.
class FixedSizeView : public View {
 public:
  FixedSizeView(const gfx::Size& size)
    : size_(size) {}

  virtual gfx::Size GetPreferredSize() {
    return size_;
  }

 private:
  const gfx::Size size_;

  DISALLOW_COPY_AND_ASSIGN(FixedSizeView);
};

class TabbedPaneTest : public testing::Test, WindowDelegate {
 public:
  TabbedPaneTest() {}

  TabbedPane* tabbed_pane_;

  void RunAllPending() {
    message_loop_.RunAllPending();
  }

 private:
  virtual void SetUp() {
    tabbed_pane_ = new TabbedPane();
    window_ = Window::CreateChromeWindow(NULL, gfx::Rect(0, 0, 100, 100), this);
    window_->Show();
  }

  virtual void TearDown() {
    window_->CloseWindow();
    message_loop_.RunAllPending();
  }

  virtual views::View* GetContentsView() {
    return tabbed_pane_;
  }

  MessageLoopForUI message_loop_;
  Window* window_;

  DISALLOW_COPY_AND_ASSIGN(TabbedPaneTest);
};

// Tests that TabbedPane::GetPreferredSize() and TabbedPane::Layout().
TEST_F(TabbedPaneTest, SizeAndLayout) {
  View* child1 = new FixedSizeView(gfx::Size(20, 10));
  tabbed_pane_->AddTab(L"tab1", child1);
  View* child2 = new FixedSizeView(gfx::Size(5, 5));
  tabbed_pane_->AddTab(L"tab2", child2);
  tabbed_pane_->SelectTabAt(0);

  // Check that the preferred size is larger than the largest child.
  gfx::Size pref(tabbed_pane_->GetPreferredSize());
  EXPECT_GT(pref.width(), 20);
  EXPECT_GT(pref.height(), 10);

  // The bounds of our children should be smaller than the tabbed pane's bounds.
  tabbed_pane_->SetBounds(0, 0, 100, 200);
  RunAllPending();
  gfx::Rect bounds(child1->bounds());
  EXPECT_GT(bounds.width(), 0);
  EXPECT_LT(bounds.width(), 100);
  EXPECT_GT(bounds.height(), 0);
  EXPECT_LT(bounds.height(), 200);

  // If we switch to the other tab, it should get assigned the same bounds.
  tabbed_pane_->SelectTabAt(1);
  EXPECT_EQ(bounds, child2->bounds());
}

} // namespace views

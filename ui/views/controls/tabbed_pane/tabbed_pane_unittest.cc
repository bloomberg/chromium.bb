// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace views {

// A view for testing that takes a fixed preferred size upon construction.
class FixedSizeView : public View {
 public:
  explicit FixedSizeView(const gfx::Size& size)
    : size_(size) {}

  // Overridden from View:
  virtual gfx::Size GetPreferredSize() OVERRIDE {
    return size_;
  }

 private:
  const gfx::Size size_;

  DISALLOW_COPY_AND_ASSIGN(FixedSizeView);
};

class TabbedPaneTest : public ViewsTestBase,
                       public WidgetDelegate {
 public:
  TabbedPaneTest() {}

  TabbedPane* tabbed_pane_;

 private:
  virtual void SetUp() OVERRIDE {
    ViewsTestBase::SetUp();
    tabbed_pane_ = new TabbedPane();
    window_ = Widget::CreateWindowWithBounds(this, gfx::Rect(0, 0, 100, 100));
    window_->Show();
  }

  virtual void TearDown() OVERRIDE {
    window_->Close();
    ViewsTestBase::TearDown();
  }

  virtual views::View* GetContentsView() OVERRIDE {
    return tabbed_pane_;
  }
  virtual views::Widget* GetWidget() OVERRIDE {
    return tabbed_pane_->GetWidget();
  }
  virtual const views::Widget* GetWidget() const OVERRIDE {
    return tabbed_pane_->GetWidget();
  }

  Widget* window_;

  DISALLOW_COPY_AND_ASSIGN(TabbedPaneTest);
};

// Tests that TabbedPane::GetPreferredSize() and TabbedPane::Layout().
TEST_F(TabbedPaneTest, SizeAndLayout) {
  View* child1 = new FixedSizeView(gfx::Size(20, 10));
  tabbed_pane_->AddTab(ASCIIToUTF16("tab1"), child1);
  View* child2 = new FixedSizeView(gfx::Size(5, 5));
  tabbed_pane_->AddTab(ASCIIToUTF16("tab2"), child2);
  tabbed_pane_->SelectTabAt(0);

  // Check that the preferred size is larger than the largest child.
  gfx::Size pref(tabbed_pane_->GetPreferredSize());
  EXPECT_GT(pref.width(), 20);
  EXPECT_GT(pref.height(), 10);

  // The bounds of our children should be smaller than the tabbed pane's bounds.
  tabbed_pane_->SetBounds(0, 0, 100, 200);
  RunPendingMessages();
  gfx::Rect bounds(child1->bounds());
  EXPECT_GT(bounds.width(), 0);
  EXPECT_LT(bounds.width(), 100);
  EXPECT_GT(bounds.height(), 0);
  EXPECT_LT(bounds.height(), 200);

  // If we switch to the other tab, it should get assigned the same bounds.
  tabbed_pane_->SelectTabAt(1);
  EXPECT_EQ(bounds, child2->bounds());
}

TEST_F(TabbedPaneTest, AddRemove) {
  View* tab0 = new View;
  tabbed_pane_->AddTab(ASCIIToUTF16("tab0"), tab0);
  EXPECT_EQ(tab0, tabbed_pane_->GetSelectedTab());
  EXPECT_EQ(0, tabbed_pane_->GetSelectedTabIndex());

  // Add more 3 tabs.
  tabbed_pane_->AddTab(ASCIIToUTF16("tab1"), new View);
  tabbed_pane_->AddTab(ASCIIToUTF16("tab2"), new View);
  tabbed_pane_->AddTab(ASCIIToUTF16("tab3"), new View);
  EXPECT_EQ(4, tabbed_pane_->GetTabCount());

  // Note: AddTab() doesn't select a tab if the tabbed pane isn't empty.

  // Select the last one.
  tabbed_pane_->SelectTabAt(tabbed_pane_->GetTabCount() - 1);
  EXPECT_EQ(3, tabbed_pane_->GetSelectedTabIndex());

  // Remove the last one.
  delete tabbed_pane_->RemoveTabAtIndex(3);
  EXPECT_EQ(3, tabbed_pane_->GetTabCount());

  // After removing the last tab, check if the tabbed pane selected the previous
  // tab.
  EXPECT_EQ(2, tabbed_pane_->GetSelectedTabIndex());

  tabbed_pane_->AddTabAtIndex(0, ASCIIToUTF16("tab4"), new View, true);

  // Assert that even adding a new tab, the tabbed pane doesn't change the
  // selection, i.e., it doesn't select the new one.
  // The last tab should remains selected, instead of the tab added at index 0.
  EXPECT_EQ(3, tabbed_pane_->GetSelectedTabIndex());

  // Now change the selected tab.
  tabbed_pane_->SelectTabAt(1);
  EXPECT_EQ(1, tabbed_pane_->GetSelectedTabIndex());

  // Remove the first one.
  delete tabbed_pane_->RemoveTabAtIndex(0);
  EXPECT_EQ(0, tabbed_pane_->GetSelectedTabIndex());
}

} // namespace views

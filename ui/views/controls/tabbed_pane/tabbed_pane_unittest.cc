// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/memory/scoped_ptr.h"
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

class TabbedPaneTest : public ViewsTestBase {
 public:
  TabbedPaneTest() {}

  void TestSizeAndLayout(TabbedPane* tabbed_pane);

  void TestAddRemove(TabbedPane* tabbed_pane);

  TabbedPane* tabbed_pane_;  // Owned by the |widget_|'s root View.

#if defined(OS_WIN) && !defined(USE_AURA)
  TabbedPane* tabbed_pane_win_;  // Owned by the |widget_|'s root View.
#endif

 private:
  virtual void SetUp() OVERRIDE;

  scoped_ptr<Widget> widget_;

  DISALLOW_COPY_AND_ASSIGN(TabbedPaneTest);
};

void TabbedPaneTest::SetUp() {
  ViewsTestBase::SetUp();
  widget_.reset(new Widget());
  Widget::InitParams params(Widget::InitParams::TYPE_POPUP);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(0, 0, 100, 100);
  widget_->Init(params);
  tabbed_pane_ = new TabbedPane();
  // In order to properly initialize the |TabbedPane| it must be added to a
  // parent view (see the ViewHierarchyChanged method of the |TabbedPane|).
  widget_->GetRootView()->AddChildView(tabbed_pane_);

#if defined(OS_WIN) && !defined(USE_AURA)
  tabbed_pane_win_ = new TabbedPane();
  tabbed_pane_win_->set_use_native_win_control(true);
  widget_->GetRootView()->AddChildView(tabbed_pane_win_);
#endif
}

void TabbedPaneTest::TestSizeAndLayout(TabbedPane* tabbed_pane) {
  View* child1 = new FixedSizeView(gfx::Size(20, 10));
  tabbed_pane->AddTab(ASCIIToUTF16("tab1"), child1);
  View* child2 = new FixedSizeView(gfx::Size(5, 5));
  tabbed_pane->AddTab(ASCIIToUTF16("tab2"), child2);
  tabbed_pane->SelectTabAt(0);

  // The |tabbed_pane_| implementation of Views has no border by default.
  // Therefore it should be as wide as the widest tab. The native Windows
  // tabbed pane has a border that used up extra space. Therefore the preferred
  // width is larger than the largest child.
  gfx::Size pref(tabbed_pane->GetPreferredSize());
  EXPECT_GE(pref.width(), 20);
  EXPECT_GT(pref.height(), 10);

  // The bounds of our children should be smaller than the tabbed pane's bounds.
  tabbed_pane->SetBounds(0, 0, 100, 200);
  RunPendingMessages();
  gfx::Rect bounds(child1->bounds());
  EXPECT_GT(bounds.width(), 0);
  // The |tabbed_pane_| has no border. Therefore the children should be as wide
  // as the |tabbed_pane_|.
  EXPECT_LE(bounds.width(), 100);
  EXPECT_GT(bounds.height(), 0);
  EXPECT_LT(bounds.height(), 200);

  // If we switch to the other tab, it should get assigned the same bounds.
  tabbed_pane->SelectTabAt(1);
  EXPECT_EQ(bounds, child2->bounds());

  // Clean up.
  delete tabbed_pane->RemoveTabAtIndex(0);
  EXPECT_EQ(1, tabbed_pane->GetTabCount());
  delete tabbed_pane->RemoveTabAtIndex(0);
  EXPECT_EQ(0, tabbed_pane->GetTabCount());
}

void TabbedPaneTest::TestAddRemove(TabbedPane* tabbed_pane) {
  View* tab0 = new View;
  tabbed_pane->AddTab(ASCIIToUTF16("tab0"), tab0);
  EXPECT_EQ(tab0, tabbed_pane->GetSelectedTab());
  EXPECT_EQ(0, tabbed_pane->GetSelectedTabIndex());

  // Add more 3 tabs.
  tabbed_pane->AddTab(ASCIIToUTF16("tab1"), new View);
  tabbed_pane->AddTab(ASCIIToUTF16("tab2"), new View);
  tabbed_pane->AddTab(ASCIIToUTF16("tab3"), new View);
  EXPECT_EQ(4, tabbed_pane->GetTabCount());

  // Note: AddTab() doesn't select a tab if the tabbed pane isn't empty.

  // Select the last one.
  tabbed_pane->SelectTabAt(tabbed_pane->GetTabCount() - 1);
  EXPECT_EQ(3, tabbed_pane->GetSelectedTabIndex());

  // Remove the last one.
  delete tabbed_pane->RemoveTabAtIndex(3);
  EXPECT_EQ(3, tabbed_pane->GetTabCount());

  // After removing the last tab, check if the tabbed pane selected the previous
  // tab.
  EXPECT_EQ(2, tabbed_pane->GetSelectedTabIndex());

  tabbed_pane->AddTabAtIndex(0, ASCIIToUTF16("tab4"), new View, true);

  // Assert that even adding a new tab, the tabbed pane doesn't change the
  // selection, i.e., it doesn't select the new one.
  // The last tab should remains selected, instead of the tab added at index 0.
  EXPECT_EQ(3, tabbed_pane->GetSelectedTabIndex());

  // Now change the selected tab.
  tabbed_pane->SelectTabAt(1);
  EXPECT_EQ(1, tabbed_pane->GetSelectedTabIndex());

  // Remove the first one.
  delete tabbed_pane->RemoveTabAtIndex(0);
  EXPECT_EQ(0, tabbed_pane->GetSelectedTabIndex());

  // Clean up the other panes.
  EXPECT_EQ(3, tabbed_pane->GetTabCount());
  delete tabbed_pane->RemoveTabAtIndex(0);
  delete tabbed_pane->RemoveTabAtIndex(0);
  delete tabbed_pane->RemoveTabAtIndex(0);
  EXPECT_EQ(0, tabbed_pane->GetTabCount());
}

// Tests TabbedPane::GetPreferredSize() and TabbedPane::Layout().
TEST_F(TabbedPaneTest, SizeAndLayout) {
  TestSizeAndLayout(tabbed_pane_);
  // TODO(markusheintz): Once replacing NativeTabbedPaneWin with
  // NativeTabbedPaneView is completed (http://crbug.com/138059), then the
  // TestSizeAndLayout method should be inlined here again and the "ifdef" part
  // should be deleted.
#if defined(OS_WIN) && !defined(USE_AURA)
  TestSizeAndLayout(tabbed_pane_win_);
#endif
}

TEST_F(TabbedPaneTest, AddRemove) {
  TestAddRemove(tabbed_pane_);
  // TODO(markusheintz): Once replacing NativeTabbedPaneWin with
  // NativeTabbedPaneView is completed (http://crbug.com/138059), then the
  // TestAddRemove method should be inlined here again and the "ifdef" part
  // should be deleted.
#if defined(OS_WIN) && !defined(USE_AURA)
  TestAddRemove(tabbed_pane_win_);
#endif
}

}  // namespace views

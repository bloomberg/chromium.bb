// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/tabbed_pane/tabbed_pane.h"

#include <memory>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/views/test/test_views.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

using base::ASCIIToUTF16;

namespace views {
namespace test {
namespace {

base::string16 DefaultTabTitle() {
  return ASCIIToUTF16("tab");
}

}  // namespace

class TabbedPaneTest : public ViewsTestBase {
 public:
  TabbedPaneTest() {
    tabbed_pane_ = std::make_unique<TabbedPane>();
    tabbed_pane_->set_owned_by_client();
  }

 protected:
  Tab* GetTabAt(int index) {
    return static_cast<Tab*>(tabbed_pane_->tab_strip_->child_at(index));
  }

  View* GetSelectedTabContentView() {
    return tabbed_pane_->GetSelectedTabContentView();
  }

  void SendKeyPressToSelectedTab(ui::KeyboardCode keyboard_code) {
    tabbed_pane_->GetSelectedTab()->OnKeyPressed(
        ui::KeyEvent(ui::ET_KEY_PRESSED, keyboard_code,
                     ui::UsLayoutKeyboardCodeToDomCode(keyboard_code), 0));
  }

  std::unique_ptr<TabbedPane> tabbed_pane_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TabbedPaneTest);
};

// Tests TabbedPane::GetPreferredSize() and TabbedPane::Layout().
TEST_F(TabbedPaneTest, SizeAndLayout) {
  View* child1 = new StaticSizedView(gfx::Size(20, 10));
  tabbed_pane_->AddTab(ASCIIToUTF16("tab1"), child1);
  View* child2 = new StaticSizedView(gfx::Size(5, 5));
  tabbed_pane_->AddTab(ASCIIToUTF16("tab2"), child2);
  tabbed_pane_->SelectTabAt(0);

  // The |tabbed_pane_| implementation of Views has no border by default.
  // Therefore it should be as wide as the widest tab. The native Windows
  // tabbed pane has a border that used up extra space. Therefore the preferred
  // width is larger than the largest child.
  gfx::Size pref(tabbed_pane_->GetPreferredSize());
  EXPECT_GE(pref.width(), 20);
  EXPECT_GT(pref.height(), 10);

  // The bounds of our children should be smaller than the tabbed pane's bounds.
  tabbed_pane_->SetBounds(0, 0, 100, 200);
  RunPendingMessages();
  gfx::Rect bounds(child1->bounds());
  EXPECT_GT(bounds.width(), 0);
  // The |tabbed_pane_| has no border. Therefore the children should be as wide
  // as the |tabbed_pane_|.
  EXPECT_LE(bounds.width(), 100);
  EXPECT_GT(bounds.height(), 0);
  EXPECT_LT(bounds.height(), 200);

  // If we switch to the other tab, it should get assigned the same bounds.
  tabbed_pane_->SelectTabAt(1);
  EXPECT_EQ(bounds, child2->bounds());
}

TEST_F(TabbedPaneTest, AddAndSelect) {
  // Add several tabs; only the first should be selected automatically.
  for (int i = 0; i < 3; ++i) {
    View* tab = new View();
    tabbed_pane_->AddTab(DefaultTabTitle(), tab);
    EXPECT_EQ(i + 1, tabbed_pane_->GetTabCount());
    EXPECT_EQ(0, tabbed_pane_->GetSelectedTabIndex());
  }

  // Select each tab.
  for (int i = 0; i < tabbed_pane_->GetTabCount(); ++i) {
    tabbed_pane_->SelectTabAt(i);
    EXPECT_EQ(i, tabbed_pane_->GetSelectedTabIndex());
  }

  // Add a tab at index 0, it should not be selected automatically.
  View* tab0 = new View();
  tabbed_pane_->AddTabAtIndex(0, ASCIIToUTF16("tab0"), tab0);
  EXPECT_NE(tab0, GetSelectedTabContentView());
  EXPECT_NE(0, tabbed_pane_->GetSelectedTabIndex());
}

TEST_F(TabbedPaneTest, ArrowKeyBindings) {
  // Add several tabs; only the first should be selected automatically.
  for (int i = 0; i < 3; ++i) {
    View* tab = new View();
    tabbed_pane_->AddTab(DefaultTabTitle(), tab);
    EXPECT_EQ(i + 1, tabbed_pane_->GetTabCount());
  }

  EXPECT_EQ(0, tabbed_pane_->GetSelectedTabIndex());

  // Right arrow should select tab 1:
  SendKeyPressToSelectedTab(ui::VKEY_RIGHT);
  EXPECT_EQ(1, tabbed_pane_->GetSelectedTabIndex());

  // Left arrow should select tab 0:
  SendKeyPressToSelectedTab(ui::VKEY_LEFT);
  EXPECT_EQ(0, tabbed_pane_->GetSelectedTabIndex());

  // Left arrow again should wrap to tab 2:
  SendKeyPressToSelectedTab(ui::VKEY_LEFT);
  EXPECT_EQ(2, tabbed_pane_->GetSelectedTabIndex());

  // Right arrow again should wrap to tab 0:
  SendKeyPressToSelectedTab(ui::VKEY_RIGHT);
  EXPECT_EQ(0, tabbed_pane_->GetSelectedTabIndex());
}

// Use TabbedPane::HandleAccessibleAction() to select tabs and make sure their
// a11y information is correct.
TEST_F(TabbedPaneTest, SelectTabWithAccessibleAction) {
  // Testing accessibility information requires the View to have a Widget.
  Widget* widget = new Widget;
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
  widget->Init(params);
  widget->GetContentsView()->AddChildView(tabbed_pane_.get());
  widget->Show();

  constexpr int kNumTabs = 3;
  for (int i = 0; i < kNumTabs; ++i) {
    tabbed_pane_->AddTab(DefaultTabTitle(), new View());
  }
  // Check the first tab is selected.
  EXPECT_EQ(0, tabbed_pane_->GetSelectedTabIndex());

  // Check the a11y information for each tab.
  for (int i = 0; i < kNumTabs; ++i) {
    ui::AXNodeData data;
    GetTabAt(i)->GetAccessibleNodeData(&data);
    SCOPED_TRACE(testing::Message() << "Tab at index: " << i);
    EXPECT_EQ(ui::AX_ROLE_TAB, data.role);
    EXPECT_EQ(DefaultTabTitle(), data.GetString16Attribute(ui::AX_ATTR_NAME));
    EXPECT_TRUE(data.HasState(ui::AX_STATE_SELECTABLE));
    EXPECT_EQ(i == 0, data.HasState(ui::AX_STATE_SELECTED));
  }

  ui::AXActionData action;
  action.action = ui::AX_ACTION_SET_SELECTION;
  // Select the first tab.

  GetTabAt(0)->HandleAccessibleAction(action);
  EXPECT_EQ(0, tabbed_pane_->GetSelectedTabIndex());

  // Select the second tab.
  GetTabAt(1)->HandleAccessibleAction(action);
  EXPECT_EQ(1, tabbed_pane_->GetSelectedTabIndex());
  // Select the second tab again.
  GetTabAt(1)->HandleAccessibleAction(action);
  EXPECT_EQ(1, tabbed_pane_->GetSelectedTabIndex());

  widget->CloseNow();
}

}  // namespace test
}  // namespace views

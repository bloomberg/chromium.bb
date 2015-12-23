// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_runner.h"

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/controls/menu/menu_delegate.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_types.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace views {
namespace test {

// Implementation of MenuDelegate that only reports the values of calls to
// OnMenuClosed.
class TestMenuDelegate : public MenuDelegate {
 public:
  TestMenuDelegate();
  ~TestMenuDelegate() override;

  int on_menu_closed_called() { return on_menu_closed_called_; }
  MenuItemView* on_menu_closed_menu() { return on_menu_closed_menu_; }
  MenuRunner::RunResult on_menu_closed_run_result() {
    return on_menu_closed_run_result_;
  }

  // MenuDelegate:
  void OnMenuClosed(MenuItemView* menu, MenuRunner::RunResult result) override;

 private:
  // The number of times OnMenuClosed was called.
  int on_menu_closed_called_;

  // The values of the last call to OnMenuClosed.
  MenuItemView* on_menu_closed_menu_;
  MenuRunner::RunResult on_menu_closed_run_result_;

  DISALLOW_COPY_AND_ASSIGN(TestMenuDelegate);
};

TestMenuDelegate::TestMenuDelegate()
    : on_menu_closed_called_(0),
      on_menu_closed_menu_(nullptr),
      on_menu_closed_run_result_(MenuRunner::MENU_DELETED) {}

TestMenuDelegate::~TestMenuDelegate() {}

void TestMenuDelegate::OnMenuClosed(MenuItemView* menu,
                                    MenuRunner::RunResult result) {
  on_menu_closed_called_++;
  on_menu_closed_menu_ = menu;
  on_menu_closed_run_result_ = result;
}

class MenuRunnerTest : public ViewsTestBase {
 public:
  MenuRunnerTest();
  ~MenuRunnerTest() override;

  // Initializes a MenuRunner with |run_types|. It takes ownership of
  // |menu_item_view_|.
  void InitMenuRunner(int32_t run_types);

  MenuItemView* menu_item_view() { return menu_item_view_; }
  TestMenuDelegate* menu_delegate() { return menu_delegate_.get(); }
  MenuRunner* menu_runner() { return menu_runner_.get(); }
  Widget* owner() { return owner_.get(); }

  // ViewsTestBase:
  void SetUp() override;
  void TearDown() override;

 private:
  // Owned by MenuRunner.
  MenuItemView* menu_item_view_;

  scoped_ptr<TestMenuDelegate> menu_delegate_;
  scoped_ptr<MenuRunner> menu_runner_;
  scoped_ptr<Widget> owner_;

  DISALLOW_COPY_AND_ASSIGN(MenuRunnerTest);
};

MenuRunnerTest::MenuRunnerTest() {}

MenuRunnerTest::~MenuRunnerTest() {}

void MenuRunnerTest::InitMenuRunner(int32_t run_types) {
  menu_runner_.reset(new MenuRunner(menu_item_view_, run_types));
}

void MenuRunnerTest::SetUp() {
  ViewsTestBase::SetUp();
  menu_delegate_.reset(new TestMenuDelegate);
  menu_item_view_ = new MenuItemView(menu_delegate_.get());

  owner_.reset(new Widget);
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  owner_->Init(params);
  owner_->Show();
}

void MenuRunnerTest::TearDown() {
  owner_->CloseNow();
  ViewsTestBase::TearDown();
}

// Tests that MenuRunner is still running after the call to RunMenuAt when
// initialized with MenuRunner::ASYNC, and that MenuDelegate is notified upon
// the closing of the menu.
TEST_F(MenuRunnerTest, AsynchronousRun) {
  InitMenuRunner(MenuRunner::ASYNC);
  MenuRunner* runner = menu_runner();
  MenuRunner::RunResult result = runner->RunMenuAt(
      owner(), nullptr, gfx::Rect(), MENU_ANCHOR_TOPLEFT, ui::MENU_SOURCE_NONE);
  EXPECT_EQ(MenuRunner::NORMAL_EXIT, result);
  EXPECT_TRUE(runner->IsRunning());

  runner->Cancel();
  EXPECT_FALSE(runner->IsRunning());
  TestMenuDelegate* delegate = menu_delegate();
  EXPECT_EQ(1, delegate->on_menu_closed_called());
  EXPECT_EQ(nullptr, delegate->on_menu_closed_menu());
  EXPECT_EQ(MenuRunner::NORMAL_EXIT, delegate->on_menu_closed_run_result());
}

}  // namespace test
}  // namespace views

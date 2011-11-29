// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/window.h"
#include "ui/aura_shell/shell.h"
#include "ui/aura_shell/shell_window_ids.h"
#include "ui/aura_shell/test/aura_shell_test_base.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace aura_shell {
namespace test {

namespace {

views::Widget* CreateTestWindow(const views::Widget::InitParams& params) {
  views::Widget* widget = new views::Widget;
  widget->Init(params);
  return widget;
}

aura::Window* GetDefaultContainer() {
  return Shell::GetInstance()->GetContainer(
        aura_shell::internal::kShellWindowId_DefaultContainer);
}

aura::Window* GetAlwaysOnTopContainer() {
  return Shell::GetInstance()->GetContainer(
        aura_shell::internal::kShellWindowId_AlwaysOnTopContainer);
}

void TestCreateWindow(views::Widget::InitParams::Type type,
                      bool always_on_top,
                      aura::Window* expected_container) {
  views::Widget::InitParams widget_params(type);
  widget_params.keep_on_top = always_on_top;

  views::Widget* widget = CreateTestWindow(widget_params);
  widget->Show();

  EXPECT_EQ(expected_container, widget->GetNativeWindow()->parent()) <<
      "TestCreateWindow: type=" << type << ", always_on_top=" << always_on_top;

  widget->Close();
}

class ModalWindow : public views::WidgetDelegateView {
 public:
  ModalWindow() {}
  virtual ~ModalWindow() {}

  // Overridden from views::WidgetDelegate:
  virtual views::View* GetContentsView() OVERRIDE {
    return this;
  }
  virtual bool CanResize() const OVERRIDE {
    return true;
  }
  virtual string16 GetWindowTitle() const OVERRIDE {
    return ASCIIToUTF16("Modal Window");
  }
  virtual bool IsModal() const OVERRIDE {
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ModalWindow);
};

}  // namespace

class ShellTest : public AuraShellTestBase {
 public:
  ShellTest() {}
  virtual ~ShellTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellTest);
};

TEST_F(ShellTest, CreateWindow) {
  // Normal window should be created in default container.
  TestCreateWindow(views::Widget::InitParams::TYPE_WINDOW,
                   false,  // always_on_top
                   GetDefaultContainer());
  TestCreateWindow(views::Widget::InitParams::TYPE_POPUP,
                   false,  // always_on_top
                   GetDefaultContainer());

  // Always-on-top window and popup are created in always-on-top container.
  TestCreateWindow(views::Widget::InitParams::TYPE_WINDOW,
                   true,  // always_on_top
                   GetAlwaysOnTopContainer());
  TestCreateWindow(views::Widget::InitParams::TYPE_POPUP,
                   true,  // always_on_top
                   GetAlwaysOnTopContainer());
}

TEST_F(ShellTest, ChangeAlwaysOnTop) {
  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_WINDOW);

  // Creates a normal window
  views::Widget* widget = CreateTestWindow(widget_params);
  widget->Show();

  // It should be in default container.
  EXPECT_EQ(GetDefaultContainer(), widget->GetNativeWindow()->parent());

  // Flip always-on-top flag.
  widget->SetAlwaysOnTop(true);
  // And it should in always on top container now.
  EXPECT_EQ(GetAlwaysOnTopContainer(), widget->GetNativeWindow()->parent());

  // Flip always-on-top flag.
  widget->SetAlwaysOnTop(false);
  // It should go back to default container.
  EXPECT_EQ(GetDefaultContainer(), widget->GetNativeWindow()->parent());

  // Set the same always-on-top flag again.
  widget->SetAlwaysOnTop(false);
  // Should have no effect and we are still in the default container.
  EXPECT_EQ(GetDefaultContainer(), widget->GetNativeWindow()->parent());

  widget->Close();
}

TEST_F(ShellTest, CreateModalWindow) {
  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_WINDOW);

  // Create a normal window.
  views::Widget* widget = CreateTestWindow(widget_params);
  widget->Show();

  // It should be in default container.
  EXPECT_EQ(GetDefaultContainer(), widget->GetNativeWindow()->parent());

  // Create a modal window.
  views::Widget* modal_widget = views::Widget::CreateWindowWithParent(
      new ModalWindow(), widget->GetNativeView());
  modal_widget->Show();

  // It should be in modal container.
  aura::Window* modal_container = Shell::GetInstance()->GetContainer(
      aura_shell::internal::kShellWindowId_ModalContainer);
  EXPECT_EQ(modal_container, modal_widget->GetNativeWindow()->parent());

  modal_widget->Close();
  widget->Close();
}

TEST_F(ShellTest, CreateLockScreenModalWindow) {
  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_WINDOW);

  // Create a normal window.
  views::Widget* widget = CreateTestWindow(widget_params);
  widget->Show();

  // It should be in default container.
  EXPECT_EQ(GetDefaultContainer(), widget->GetNativeWindow()->parent());

  // Create a LockScreen window.
  views::Widget* lock_widget = CreateTestWindow(widget_params);
  aura_shell::Shell::GetInstance()->GetContainer(
      aura_shell::internal::kShellWindowId_LockScreenContainer)->
      AddChild(lock_widget->GetNativeView());
  lock_widget->Show();

  // It should be in LockScreen container.
  aura::Window* lock_screen = Shell::GetInstance()->GetContainer(
      aura_shell::internal::kShellWindowId_LockScreenContainer);
  EXPECT_EQ(lock_screen, lock_widget->GetNativeWindow()->parent());

  // Create a modal window with a lock window as parent.
  views::Widget* lock_modal_widget = views::Widget::CreateWindowWithParent(
      new ModalWindow(), lock_widget->GetNativeView());
  lock_modal_widget->Show();

  // It should be in LockScreen modal container.
  aura::Window* lock_modal_container = Shell::GetInstance()->GetContainer(
      aura_shell::internal::kShellWindowId_LockModalContainer);
  EXPECT_EQ(lock_modal_container,
            lock_modal_widget->GetNativeWindow()->parent());

  // Create a modal window with a normal window as parent.
  views::Widget* modal_widget = views::Widget::CreateWindowWithParent(
      new ModalWindow(), widget->GetNativeView());
  modal_widget->Show();

  // It should be in non-LockScreen modal container.
  aura::Window* modal_container = Shell::GetInstance()->GetContainer(
      aura_shell::internal::kShellWindowId_ModalContainer);
  EXPECT_EQ(modal_container, modal_widget->GetNativeWindow()->parent());

  modal_widget->Close();
  lock_modal_widget->Close();
  lock_widget->Close();
  widget->Close();
}

}  // namespace test
}  // namespace aura_shell

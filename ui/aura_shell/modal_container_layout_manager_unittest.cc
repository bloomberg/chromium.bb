// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/modal_container_layout_manager.h"

#include "base/compiler_specific.h"
#include "ui/aura/desktop.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/window.h"
#include "ui/aura_shell/shell.h"
#include "ui/aura_shell/shell_window_ids.h"
#include "ui/aura_shell/test/aura_shell_test_base.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace aura_shell {
namespace test {

namespace {

aura::Window* GetModalContainer() {
  return Shell::GetInstance()->GetContainer(
      aura_shell::internal::kShellWindowId_ModalContainer);
}

aura::Window* GetDefaultContainer() {
  return Shell::GetInstance()->GetContainer(
      aura_shell::internal::kShellWindowId_DefaultContainer);
}

class TestWindow : public views::WidgetDelegateView {
 public:
  explicit TestWindow(bool modal) : modal_(modal) {}
  virtual ~TestWindow() {}

  static aura::Window* OpenTestWindow(aura::Window* parent, bool modal) {
    DCHECK(!modal || (modal && parent));
    views::Widget* widget =
        views::Widget::CreateWindowWithParent(new TestWindow(modal), parent);
    widget->Show();
    return widget->GetNativeView();
  }

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize() OVERRIDE {
    return gfx::Size(50, 50);
  }

  // Overridden from views::WidgetDelegate:
  virtual views::View* GetContentsView() OVERRIDE {
    return this;
  }
  virtual bool IsModal() const OVERRIDE {
    return modal_;
  }

 private:
  bool modal_;

  DISALLOW_COPY_AND_ASSIGN(TestWindow);
};

class TransientWindowObserver : public aura::WindowObserver {
 public:
  TransientWindowObserver() : destroyed_(false) {}
  virtual ~TransientWindowObserver() {}

  bool destroyed() const { return destroyed_; }

  // Overridden from aura::WindowObserver:
  virtual void OnWindowDestroyed(aura::Window* window) OVERRIDE {
    destroyed_ = true;
  }

 private:
  bool destroyed_;

  DISALLOW_COPY_AND_ASSIGN(TransientWindowObserver);
};

}  // namespace

typedef aura_shell::test::AuraShellTestBase ModalContainerLayoutManagerTest;

TEST_F(ModalContainerLayoutManagerTest, NonModalTransient) {
  scoped_ptr<aura::Window> parent(TestWindow::OpenTestWindow(NULL, false));
  aura::Window* transient = TestWindow::OpenTestWindow(parent.get(), false);
  TransientWindowObserver destruction_observer;
  transient->AddObserver(&destruction_observer);

  EXPECT_EQ(parent.get(), transient->transient_parent());
  EXPECT_EQ(GetDefaultContainer(), transient->parent());

  // The transient should be destroyed with its parent.
  parent.reset();
  EXPECT_TRUE(destruction_observer.destroyed());
}

TEST_F(ModalContainerLayoutManagerTest, ModalTransient) {
  scoped_ptr<aura::Window> parent(TestWindow::OpenTestWindow(NULL, false));
  // parent should be active.
  EXPECT_EQ(parent.get(), aura::Desktop::GetInstance()->active_window());

  aura::Window* t1 = TestWindow::OpenTestWindow(parent.get(), true);
  TransientWindowObserver do1;
  t1->AddObserver(&do1);

  EXPECT_EQ(parent.get(), t1->transient_parent());
  EXPECT_EQ(GetModalContainer(), t1->parent());

  // t1 should now be active.
  EXPECT_EQ(t1, aura::Desktop::GetInstance()->active_window());

  // Attempting to click the parent should result in no activation change.
  aura::test::EventGenerator e1(parent.get());
  e1.ClickLeftButton();
  EXPECT_EQ(t1, aura::Desktop::GetInstance()->active_window());

  // Now open another modal transient parented to the original modal transient.
  aura::Window* t2 = TestWindow::OpenTestWindow(t1, true);
  TransientWindowObserver do2;
  t2->AddObserver(&do2);

  EXPECT_EQ(t2, aura::Desktop::GetInstance()->active_window());

  EXPECT_EQ(t1, t2->transient_parent());
  EXPECT_EQ(GetModalContainer(), t2->parent());

  // t2 should still be active, even after clicking on t1.
  aura::test::EventGenerator e2(t1);
  e2.ClickLeftButton();
  EXPECT_EQ(t2, aura::Desktop::GetInstance()->active_window());

  // Both transients should be destroyed with parent.
  parent.reset();
  EXPECT_TRUE(do1.destroyed());
  EXPECT_TRUE(do2.destroyed());
}

// Tests that we can activate an unrelated window after a modal window is closed
// for a window.
TEST_F(ModalContainerLayoutManagerTest, CanActivateAfterEndModalSession) {
  scoped_ptr<aura::Window> unrelated(TestWindow::OpenTestWindow(NULL, false));
  unrelated->SetBounds(gfx::Rect(100, 100, 50, 50));
  scoped_ptr<aura::Window> parent(TestWindow::OpenTestWindow(NULL, false));
  // parent should be active.
  EXPECT_EQ(parent.get(), aura::Desktop::GetInstance()->active_window());

  scoped_ptr<aura::Window> transient(
      TestWindow::OpenTestWindow(parent.get(), true));
  // t1 should now be active.
  EXPECT_EQ(transient.get(), aura::Desktop::GetInstance()->active_window());

  // Attempting to click the parent should result in no activation change.
  aura::test::EventGenerator e1(parent.get());
  e1.ClickLeftButton();
  EXPECT_EQ(transient.get(), aura::Desktop::GetInstance()->active_window());

  // Now close the transient.
  transient.reset();

  // parent should now be active again.
  EXPECT_EQ(parent.get(), aura::Desktop::GetInstance()->active_window());

  // Attempting to click unrelated should activate it.
  aura::test::EventGenerator e2(unrelated.get());
  e2.ClickLeftButton();
  EXPECT_EQ(unrelated.get(), aura::Desktop::GetInstance()->active_window());
}

}  // namespace test
}  // namespace aura_shell

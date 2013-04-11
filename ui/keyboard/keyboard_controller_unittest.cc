// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/aura_test_helper.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer_type.h"
#include "ui/gfx/rect.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_controller_proxy.h"

namespace keyboard {
namespace {

// An event handler that focuses a window when it is clicked/touched on. This is
// used to match the focus manger behaviour in ash and views.
class TestFocusController : public ui::EventHandler {
 public:
  explicit TestFocusController(aura::RootWindow* root)
      : root_(root) {
    root_->AddPreTargetHandler(this);
  }

  virtual ~TestFocusController() {
    root_->RemovePreTargetHandler(this);
  }

 private:
  // Overridden from ui::EventHandler:
  virtual void OnEvent(ui::Event* event) OVERRIDE {
    aura::Window* target = static_cast<aura::Window*>(event->target());
    if (event->type() == ui::ET_MOUSE_PRESSED ||
        event->type() == ui::ET_TOUCH_PRESSED) {
      aura::client::GetFocusClient(target)->FocusWindow(target);
    }
  }

  aura::Window* root_;
  DISALLOW_COPY_AND_ASSIGN(TestFocusController);
};

class KeyboardControllerTest : public testing::Test {
 public:
  KeyboardControllerTest() {}
  virtual ~KeyboardControllerTest() {}

  virtual void SetUp() OVERRIDE {
    aura_test_helper_.reset(new aura::test::AuraTestHelper(&message_loop_));
    aura_test_helper_->SetUp();
    focus_controller_.reset(new TestFocusController(root_window()));
  }

  virtual void TearDown() OVERRIDE {
    aura_test_helper_->TearDown();
  }

  aura::RootWindow* root_window() { return aura_test_helper_->root_window(); }

 protected:
  base::MessageLoopForUI message_loop_;
  scoped_ptr<aura::test::AuraTestHelper> aura_test_helper_;
  scoped_ptr<TestFocusController> focus_controller_;

 private:
  DISALLOW_COPY_AND_ASSIGN(KeyboardControllerTest);
};

class TestKeyboardControllerProxy : public KeyboardControllerProxy {
 public:
  TestKeyboardControllerProxy() : window_(new aura::Window(&delegate_)) {
    window_->Init(ui::LAYER_NOT_DRAWN);
    window_->set_owned_by_parent(false);
  }

  virtual ~TestKeyboardControllerProxy() {
    // Destroy the window before the delegate.
    window_.reset();
  }

  // Overridden from KeyboardControllerProxy:
  virtual aura::Window* GetKeyboardWindow() OVERRIDE { return window_.get(); }

 private:
  scoped_ptr<aura::Window> window_;
  aura::test::TestWindowDelegate delegate_;

  DISALLOW_COPY_AND_ASSIGN(TestKeyboardControllerProxy);
};

// Keeps a count of all the events a window receives.
class EventObserver : public ui::EventHandler {
 public:
  EventObserver() {}
  virtual ~EventObserver() {}

  int GetEventCount(ui::EventType type) {
    return event_counts_[type];
  }

 private:
  // Overridden from ui::EventHandler:
  virtual void OnEvent(ui::Event* event) OVERRIDE {
    ui::EventHandler::OnEvent(event);
    event_counts_[event->type()]++;
  }

  std::map<ui::EventType, int> event_counts_;
  DISALLOW_COPY_AND_ASSIGN(EventObserver);
};

}  // namespace

TEST_F(KeyboardControllerTest, KeyboardSize) {
  KeyboardControllerProxy* proxy = new TestKeyboardControllerProxy();
  KeyboardController controller(proxy);

  scoped_ptr<aura::Window> container(controller.GetContainerWindow());
  gfx::Rect bounds(0, 0, 100, 100);
  container->SetBounds(bounds);

  const gfx::Rect& before_bounds = proxy->GetKeyboardWindow()->bounds();
  gfx::Rect new_bounds(
      before_bounds.x(), before_bounds.y(),
      before_bounds.width() / 2, before_bounds.height() / 2);

  // The KeyboardController's LayoutManager shouldn't let this happen
  proxy->GetKeyboardWindow()->SetBounds(new_bounds);
  ASSERT_EQ(before_bounds, proxy->GetKeyboardWindow()->bounds());
}

// Tests that tapping/clicking inside the keyboard does not give it focus.
TEST_F(KeyboardControllerTest, ClickDoesNotFocusKeyboard) {
  const gfx::Rect& root_bounds = root_window()->bounds();
  aura::test::EventCountDelegate delegate;
  scoped_ptr<aura::Window> window(new aura::Window(&delegate));
  window->Init(ui::LAYER_NOT_DRAWN);
  window->SetBounds(root_bounds);
  root_window()->AddChild(window.get());
  window->Show();
  window->Focus();

  KeyboardControllerProxy* proxy = new TestKeyboardControllerProxy();
  KeyboardController controller(proxy);

  scoped_ptr<aura::Window> keyboard_container(controller.GetContainerWindow());
  keyboard_container->SetBounds(root_bounds);

  root_window()->AddChild(keyboard_container.get());
  keyboard_container->Show();

  root_window()->StackChildAtTop(keyboard_container.get());
  EXPECT_TRUE(window->IsVisible());
  EXPECT_TRUE(keyboard_container->IsVisible());
  EXPECT_TRUE(window->HasFocus());
  EXPECT_FALSE(keyboard_container->HasFocus());

  // Click on the keyboard. Make sure the keyboard receives the event, but does
  // not get focus.
  EventObserver observer;
  keyboard_container->AddPreTargetHandler(&observer);

  aura::test::EventGenerator generator(root_window());
  generator.MoveMouseTo(proxy->GetKeyboardWindow()->bounds().CenterPoint());
  generator.ClickLeftButton();
  EXPECT_TRUE(window->HasFocus());
  EXPECT_FALSE(keyboard_container->HasFocus());
  EXPECT_EQ("0 0", delegate.GetMouseButtonCountsAndReset());
  EXPECT_EQ(1, observer.GetEventCount(ui::ET_MOUSE_PRESSED));
  EXPECT_EQ(1, observer.GetEventCount(ui::ET_MOUSE_RELEASED));

  // Click outside of the keyboard. It should reach the window behind.
  generator.MoveMouseTo(gfx::Point());
  generator.ClickLeftButton();
  EXPECT_EQ("1 1", delegate.GetMouseButtonCountsAndReset());
}

}  // namespace keyboard

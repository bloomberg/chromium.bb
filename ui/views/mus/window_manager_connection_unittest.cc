// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/window_manager_connection.h"

#include <memory>

#include "base/message_loop/message_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/views/pointer_watcher.h"
#include "ui/views/test/scoped_views_test_helper.h"

namespace views {
namespace {

class TestPointerWatcher : public PointerWatcher {
 public:
  TestPointerWatcher() {}
  ~TestPointerWatcher() override {}

  ui::Event* last_event() { return last_event_.get(); }

  void Reset() { last_event_.reset(); }

  // PointerWatcher:
  void OnMousePressed(const ui::MouseEvent& event) override {
    last_event_ = ui::Event::Clone(event);
  }
  void OnTouchPressed(const ui::TouchEvent& event) override {
    last_event_ = ui::Event::Clone(event);
  }

 private:
  std::unique_ptr<ui::Event> last_event_;

  DISALLOW_COPY_AND_ASSIGN(TestPointerWatcher);
};

}  // namespace

class WindowManagerConnectionTest : public testing::Test {
 public:
  WindowManagerConnectionTest() {}
  ~WindowManagerConnectionTest() override {}

  void OnEventObserved(const ui::Event& event) {
    WindowManagerConnection::Get()->OnEventObserved(event);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WindowManagerConnectionTest);
};

TEST_F(WindowManagerConnectionTest, PointerWatcher) {
  base::MessageLoop message_loop(base::MessageLoop::TYPE_UI);
  ScopedViewsTestHelper helper;
  WindowManagerConnection* connection = WindowManagerConnection::Get();
  ASSERT_TRUE(connection);
  ui::MouseEvent mouse_pressed(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                               base::TimeDelta(), ui::EF_NONE, 0);
  ui::TouchEvent touch_pressed(ui::ET_TOUCH_PRESSED, gfx::Point(), 1,
                               base::TimeDelta());
  ui::KeyEvent key_pressed(ui::ET_KEY_PRESSED, ui::VKEY_A, 0);

  // PointerWatchers receive mouse events.
  TestPointerWatcher watcher1;
  connection->AddPointerWatcher(&watcher1);
  OnEventObserved(mouse_pressed);
  EXPECT_EQ(ui::ET_MOUSE_PRESSED, watcher1.last_event()->type());
  watcher1.Reset();

  // PointerWatchers receive touch events.
  OnEventObserved(touch_pressed);
  EXPECT_EQ(ui::ET_TOUCH_PRESSED, watcher1.last_event()->type());
  watcher1.Reset();

  // PointerWatchers do not receive key events.
  OnEventObserved(key_pressed);
  EXPECT_FALSE(watcher1.last_event());
  watcher1.Reset();

  // Two PointerWatchers can both receive a single observed event.
  TestPointerWatcher watcher2;
  connection->AddPointerWatcher(&watcher2);
  OnEventObserved(mouse_pressed);
  EXPECT_EQ(ui::ET_MOUSE_PRESSED, watcher1.last_event()->type());
  EXPECT_EQ(ui::ET_MOUSE_PRESSED, watcher2.last_event()->type());
  watcher1.Reset();
  watcher2.Reset();

  // Removing the first PointerWatcher stops sending events to it.
  connection->RemovePointerWatcher(&watcher1);
  OnEventObserved(mouse_pressed);
  EXPECT_FALSE(watcher1.last_event());
  EXPECT_EQ(ui::ET_MOUSE_PRESSED, watcher2.last_event()->type());
  watcher1.Reset();
  watcher2.Reset();

  // Removing the last PointerWatcher stops sending events to it.
  connection->RemovePointerWatcher(&watcher2);
  OnEventObserved(mouse_pressed);
  EXPECT_FALSE(watcher1.last_event());
  EXPECT_FALSE(watcher2.last_event());
}

}  // namespace views

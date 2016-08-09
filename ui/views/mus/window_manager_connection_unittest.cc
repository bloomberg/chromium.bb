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

  ui::PointerEvent* last_event_observed() { return last_event_observed_.get(); }

  void Reset() { last_event_observed_.reset(); }

  // PointerWatcher:
  void OnPointerEventObserved(const ui::PointerEvent& event,
                              const gfx::Point& location_in_screen,
                              Widget* target) override {
    last_event_observed_.reset(new ui::PointerEvent(event));
  }

 private:
  std::unique_ptr<ui::PointerEvent> last_event_observed_;

  DISALLOW_COPY_AND_ASSIGN(TestPointerWatcher);
};

}  // namespace

class WindowManagerConnectionTest : public testing::Test {
 public:
  WindowManagerConnectionTest() {}
  ~WindowManagerConnectionTest() override {}

  void OnPointerEventObserved(const ui::PointerEvent& event) {
    WindowManagerConnection::Get()->OnPointerEventObserved(event, nullptr);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WindowManagerConnectionTest);
};

TEST_F(WindowManagerConnectionTest, PointerWatcherNoMove) {
  base::MessageLoop message_loop(base::MessageLoop::TYPE_UI);
  ScopedViewsTestHelper helper;
  WindowManagerConnection* connection = WindowManagerConnection::Get();
  ASSERT_TRUE(connection);

  ui::PointerEvent pointer_event_down(
      ui::ET_POINTER_DOWN, gfx::Point(), gfx::Point(), ui::EF_NONE, 1,
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH),
      base::TimeTicks());
  ui::PointerEvent pointer_event_up(
      ui::ET_POINTER_UP, gfx::Point(), gfx::Point(), ui::EF_NONE, 1,
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_MOUSE),
      base::TimeTicks());

  // PointerWatchers receive pointer down events.
  TestPointerWatcher watcher1;
  connection->AddPointerWatcher(&watcher1, false);
  OnPointerEventObserved(pointer_event_down);
  EXPECT_EQ(ui::ET_POINTER_DOWN, watcher1.last_event_observed()->type());
  watcher1.Reset();

  // PointerWatchers receive pointer up events.
  OnPointerEventObserved(pointer_event_up);
  EXPECT_EQ(ui::ET_POINTER_UP, watcher1.last_event_observed()->type());
  watcher1.Reset();

  // Two PointerWatchers can both receive a single observed event.
  TestPointerWatcher watcher2;
  connection->AddPointerWatcher(&watcher2, false);
  OnPointerEventObserved(pointer_event_down);
  EXPECT_EQ(ui::ET_POINTER_DOWN, watcher1.last_event_observed()->type());
  EXPECT_EQ(ui::ET_POINTER_DOWN, watcher2.last_event_observed()->type());
  watcher1.Reset();
  watcher2.Reset();

  // Removing the first PointerWatcher stops sending events to it.
  connection->RemovePointerWatcher(&watcher1);
  OnPointerEventObserved(pointer_event_down);
  EXPECT_FALSE(watcher1.last_event_observed());
  EXPECT_EQ(ui::ET_POINTER_DOWN, watcher2.last_event_observed()->type());
  watcher1.Reset();
  watcher2.Reset();

  // Removing the last PointerWatcher stops sending events to it.
  connection->RemovePointerWatcher(&watcher2);
  OnPointerEventObserved(pointer_event_down);
  EXPECT_FALSE(watcher1.last_event_observed());
  EXPECT_FALSE(watcher2.last_event_observed());
}

TEST_F(WindowManagerConnectionTest, PointerWatcherMove) {
  base::MessageLoop message_loop(base::MessageLoop::TYPE_UI);
  ScopedViewsTestHelper helper;
  WindowManagerConnection* connection = WindowManagerConnection::Get();
  ASSERT_TRUE(connection);

  ui::PointerEvent pointer_event_down(
      ui::ET_POINTER_DOWN, gfx::Point(), gfx::Point(), ui::EF_NONE, 1,
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH),
      base::TimeTicks());
  ui::PointerEvent pointer_event_move(
      ui::ET_POINTER_MOVED, gfx::Point(), gfx::Point(), ui::EF_NONE, 1,
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH),
      base::TimeTicks());

  // PointerWatchers receive pointer down events.
  TestPointerWatcher watcher1;
  connection->AddPointerWatcher(&watcher1, true);
  OnPointerEventObserved(pointer_event_down);
  EXPECT_EQ(ui::ET_POINTER_DOWN, watcher1.last_event_observed()->type());
  watcher1.Reset();

  // PointerWatchers receive pointer move events.
  OnPointerEventObserved(pointer_event_move);
  EXPECT_EQ(ui::ET_POINTER_MOVED, watcher1.last_event_observed()->type());
  watcher1.Reset();

  // Two PointerWatchers can both receive a single observed event.
  TestPointerWatcher watcher2;
  connection->AddPointerWatcher(&watcher2, true);
  OnPointerEventObserved(pointer_event_move);
  EXPECT_EQ(ui::ET_POINTER_MOVED, watcher1.last_event_observed()->type());
  EXPECT_EQ(ui::ET_POINTER_MOVED, watcher2.last_event_observed()->type());
  watcher1.Reset();
  watcher2.Reset();

  // Removing the first PointerWatcher stops sending events to it.
  connection->RemovePointerWatcher(&watcher1);
  OnPointerEventObserved(pointer_event_move);
  EXPECT_FALSE(watcher1.last_event_observed());
  EXPECT_EQ(ui::ET_POINTER_MOVED, watcher2.last_event_observed()->type());
  watcher1.Reset();
  watcher2.Reset();

  // Removing the last PointerWatcher stops sending events to it.
  connection->RemovePointerWatcher(&watcher2);
  OnPointerEventObserved(pointer_event_move);
  EXPECT_FALSE(watcher1.last_event_observed());
  EXPECT_FALSE(watcher2.last_event_observed());
}

}  // namespace views

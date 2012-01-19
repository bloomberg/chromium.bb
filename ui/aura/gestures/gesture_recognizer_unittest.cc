// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/event.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"

namespace aura {
namespace test {

namespace {

// A delegate that keeps track of gesture events.
class GestureEventConsumeDelegate : public TestWindowDelegate {
 public:
  GestureEventConsumeDelegate()
      : tap_(false),
        tap_down_(false),
        double_tap_(false),
        scroll_begin_(false),
        scroll_update_(false),
        scroll_end_(false),
        scroll_x_(0),
        scroll_y_(0) {
  }

  virtual ~GestureEventConsumeDelegate() {}

  void Reset() {
    tap_ = false;
    tap_down_ = false;
    double_tap_ = false;
    scroll_begin_ = false;
    scroll_update_ = false;
    scroll_end_ = false;

    scroll_x_ = 0;
    scroll_y_ = 0;
  }

  bool tap() const { return tap_; }
  bool tap_down() const { return tap_down_; }
  bool double_tap() const { return double_tap_; }
  bool scroll_begin() const { return scroll_begin_; }
  bool scroll_update() const { return scroll_update_; }
  bool scroll_end() const { return scroll_end_; }

  float scroll_x() const { return scroll_x_; }
  float scroll_y() const { return scroll_y_; }

  virtual ui::GestureStatus OnGestureEvent(GestureEvent* gesture) OVERRIDE {
    switch (gesture->type()) {
      case ui::ET_GESTURE_TAP:
        tap_ = true;
        break;
      case ui::ET_GESTURE_TAP_DOWN:
        tap_down_ = true;
        break;
      case ui::ET_GESTURE_DOUBLE_TAP:
        double_tap_ = true;
        break;
      case ui::ET_GESTURE_SCROLL_BEGIN:
        scroll_begin_ = true;
        break;
      case ui::ET_GESTURE_SCROLL_UPDATE:
        scroll_update_ = true;
        scroll_x_ += gesture->delta_x();
        scroll_y_ += gesture->delta_y();
        break;
      case ui::ET_GESTURE_SCROLL_END:
        scroll_end_ = true;
        break;
      default:
        NOTREACHED();
    }
    return ui::GESTURE_STATUS_CONSUMED;
  }

 private:
  bool tap_;
  bool tap_down_;
  bool double_tap_;
  bool scroll_begin_;
  bool scroll_update_;
  bool scroll_end_;

  float scroll_x_;
  float scroll_y_;

  DISALLOW_COPY_AND_ASSIGN(GestureEventConsumeDelegate);
};

// A delegate that ignores gesture events but keeps track of [synthetic] mouse
// events.
class GestureEventSynthDelegate : public TestWindowDelegate {
 public:
  GestureEventSynthDelegate()
      : mouse_enter_(false),
        mouse_exit_(false),
        mouse_press_(false),
        mouse_release_(false),
        mouse_move_(false) {
  }

  void Reset() {
    mouse_enter_ = false;
    mouse_exit_ = false;
    mouse_press_ = false;
    mouse_release_ = false;
    mouse_move_ = false;
  }

  bool mouse_enter() const { return mouse_enter_; }
  bool mouse_exit() const { return mouse_exit_; }
  bool mouse_press() const { return mouse_press_; }
  bool mouse_move() const { return mouse_move_; }
  bool mouse_release() const { return mouse_release_; }

  virtual bool OnMouseEvent(MouseEvent* event) OVERRIDE {
    switch (event->type()) {
      case ui::ET_MOUSE_PRESSED:
        mouse_press_ = true;
        break;
      case ui::ET_MOUSE_RELEASED:
        mouse_release_ = true;
        break;
      case ui::ET_MOUSE_MOVED:
        mouse_move_ = true;
        break;
      case ui::ET_MOUSE_ENTERED:
        mouse_enter_ = true;
        break;
      case ui::ET_MOUSE_EXITED:
        mouse_exit_ = true;
        break;
      default:
        NOTREACHED();
    }
    return true;
  }

 private:
  bool mouse_enter_;
  bool mouse_exit_;
  bool mouse_press_;
  bool mouse_release_;
  bool mouse_move_;

  DISALLOW_COPY_AND_ASSIGN(GestureEventSynthDelegate);
};

}  // namespace

typedef AuraTestBase GestureRecognizerTest;

// Check that appropriate touch events generate tap gesture events.
TEST_F(GestureRecognizerTest, GestureEventTap) {
  scoped_ptr<GestureEventConsumeDelegate> delegate(
      new GestureEventConsumeDelegate());
  const int kWindowWidth = 123;
  const int kWindowHeight = 45;
  gfx::Rect bounds(100, 200, kWindowWidth, kWindowHeight);
  scoped_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, NULL));

  delegate->Reset();
  TouchEvent press(ui::ET_TOUCH_PRESSED, gfx::Point(101, 201), 0);
  RootWindow::GetInstance()->DispatchTouchEvent(&press);
  EXPECT_FALSE(delegate->tap());
  EXPECT_TRUE(delegate->tap_down());
  EXPECT_FALSE(delegate->double_tap());
  EXPECT_FALSE(delegate->scroll_begin());
  EXPECT_FALSE(delegate->scroll_update());
  EXPECT_FALSE(delegate->scroll_end());

  // Make sure there is enough delay before the touch is released so that it is
  // recognized as a tap.
  delegate->Reset();
  TouchEvent release(ui::ET_TOUCH_RELEASED, gfx::Point(101, 201), 0);
  Event::TestApi test_release(&release);
  test_release.set_time_stamp(press.time_stamp() +
                              base::TimeDelta::FromMilliseconds(50));
  RootWindow::GetInstance()->DispatchTouchEvent(&release);
  EXPECT_TRUE(delegate->tap());
  EXPECT_FALSE(delegate->tap_down());
  EXPECT_FALSE(delegate->double_tap());
  EXPECT_FALSE(delegate->scroll_begin());
  EXPECT_FALSE(delegate->scroll_update());
  EXPECT_FALSE(delegate->scroll_end());
}

// Check that appropriate touch events generate scroll gesture events.
TEST_F(GestureRecognizerTest, GestureEventScroll) {
  scoped_ptr<GestureEventConsumeDelegate> delegate(
      new GestureEventConsumeDelegate());
  const int kWindowWidth = 123;
  const int kWindowHeight = 45;
  gfx::Rect bounds(100, 200, kWindowWidth, kWindowHeight);
  scoped_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, NULL));

  delegate->Reset();
  TouchEvent press(ui::ET_TOUCH_PRESSED, gfx::Point(101, 201), 0);
  RootWindow::GetInstance()->DispatchTouchEvent(&press);
  EXPECT_FALSE(delegate->tap());
  EXPECT_TRUE(delegate->tap_down());
  EXPECT_FALSE(delegate->double_tap());
  EXPECT_FALSE(delegate->scroll_begin());
  EXPECT_FALSE(delegate->scroll_update());
  EXPECT_FALSE(delegate->scroll_end());

  // Move the touch-point enough so that it is considered as a scroll. This
  // should generate both SCROLL_BEGIN and SCROLL_UPDATE gestures.
  delegate->Reset();
  TouchEvent move(ui::ET_TOUCH_MOVED, gfx::Point(130, 201), 0);
  RootWindow::GetInstance()->DispatchTouchEvent(&move);
  EXPECT_FALSE(delegate->tap());
  EXPECT_FALSE(delegate->tap_down());
  EXPECT_FALSE(delegate->double_tap());
  EXPECT_TRUE(delegate->scroll_begin());
  EXPECT_TRUE(delegate->scroll_update());
  EXPECT_FALSE(delegate->scroll_end());
  EXPECT_EQ(29, delegate->scroll_x());
  EXPECT_EQ(0, delegate->scroll_y());

  // Release the touch. This should end the scroll.
  delegate->Reset();
  TouchEvent release(ui::ET_TOUCH_RELEASED, gfx::Point(101, 201), 0);
  Event::TestApi test_release(&release);
  test_release.set_time_stamp(press.time_stamp() +
                              base::TimeDelta::FromMilliseconds(50));
  RootWindow::GetInstance()->DispatchTouchEvent(&release);
  EXPECT_FALSE(delegate->tap());
  EXPECT_FALSE(delegate->tap_down());
  EXPECT_FALSE(delegate->double_tap());
  EXPECT_FALSE(delegate->scroll_begin());
  EXPECT_FALSE(delegate->scroll_update());
  EXPECT_TRUE(delegate->scroll_end());
}

// Check that unprocessed gesture events generate appropriate synthetic mouse
// events.
TEST_F(GestureRecognizerTest, GestureTapSyntheticMouse) {
  scoped_ptr<GestureEventSynthDelegate> delegate(
      new GestureEventSynthDelegate());
  scoped_ptr<Window> window(CreateTestWindowWithDelegate(delegate.get(), -1234,
        gfx::Rect(0, 0, 123, 45), NULL));

  delegate->Reset();
  GestureEvent tap(ui::ET_GESTURE_TAP, 20, 20, 0, base::Time::Now(), 0, 0);
  RootWindow::GetInstance()->DispatchGestureEvent(&tap);
  EXPECT_TRUE(delegate->mouse_enter());
  EXPECT_TRUE(delegate->mouse_press());
  EXPECT_TRUE(delegate->mouse_release());
  EXPECT_TRUE(delegate->mouse_exit());
}

}  // namespace test
}  // namespace aura

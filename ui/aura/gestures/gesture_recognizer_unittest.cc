// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_vector.h"
#include "base/timer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/event.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/base/gestures/gesture_configuration.h"
#include "ui/base/gestures/gesture_recognizer_impl.h"
#include "ui/base/gestures/gesture_sequence.h"
#include "ui/base/gestures/gesture_types.h"
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
        pinch_begin_(false),
        pinch_update_(false),
        pinch_end_(false),
        long_press_(false),
        fling_(false),
        three_finger_swipe_(false),
        scroll_x_(0),
        scroll_y_(0),
        velocity_x_(0),
        velocity_y_(0) {
  }

  virtual ~GestureEventConsumeDelegate() {}

  void Reset() {
    tap_ = false;
    tap_down_ = false;
    double_tap_ = false;
    scroll_begin_ = false;
    scroll_update_ = false;
    scroll_end_ = false;
    pinch_begin_ = false;
    pinch_update_ = false;
    pinch_end_ = false;
    long_press_ = false;
    fling_ = false;
    three_finger_swipe_ = false;

    scroll_begin_position_.SetPoint(0, 0);

    scroll_x_ = 0;
    scroll_y_ = 0;
    velocity_x_ = 0;
    velocity_y_ = 0;
  }

  bool tap() const { return tap_; }
  bool tap_down() const { return tap_down_; }
  bool double_tap() const { return double_tap_; }
  bool scroll_begin() const { return scroll_begin_; }
  bool scroll_update() const { return scroll_update_; }
  bool scroll_end() const { return scroll_end_; }
  bool pinch_begin() const { return pinch_begin_; }
  bool pinch_update() const { return pinch_update_; }
  bool pinch_end() const { return pinch_end_; }
  bool long_press() const { return long_press_; }
  bool fling() const { return fling_; }
  bool three_finger_swipe() const { return three_finger_swipe_; }

  const gfx::Point scroll_begin_position() const {
    return scroll_begin_position_;
  }

  float scroll_x() const { return scroll_x_; }
  float scroll_y() const { return scroll_y_; }
  int touch_id() const { return touch_id_; }
  float velocity_x() const { return velocity_x_; }
  float velocity_y() const { return velocity_y_; }

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
        scroll_begin_position_ = gesture->location();
        break;
      case ui::ET_GESTURE_SCROLL_UPDATE:
        scroll_update_ = true;
        scroll_x_ += gesture->delta_x();
        scroll_y_ += gesture->delta_y();
        break;
      case ui::ET_GESTURE_SCROLL_END:
        velocity_x_ = gesture->delta_x();
        velocity_y_ = gesture->delta_y();
        scroll_end_ = true;
        break;
      case ui::ET_GESTURE_PINCH_BEGIN:
        pinch_begin_ = true;
        break;
      case ui::ET_GESTURE_PINCH_UPDATE:
        pinch_update_ = true;
        break;
      case ui::ET_GESTURE_PINCH_END:
        pinch_end_ = true;
        break;
      case ui::ET_GESTURE_LONG_PRESS:
        long_press_ = true;
        touch_id_ = gesture->delta_x();
        break;
      case ui::ET_SCROLL_FLING_START:
        EXPECT_TRUE(scroll_end_);
        EXPECT_TRUE(velocity_x_ != 0 || velocity_y_ != 0);
        fling_ = true;
        break;
      case ui::ET_GESTURE_THREE_FINGER_SWIPE:
        three_finger_swipe_ = true;
        velocity_x_ = gesture->delta_x();
        velocity_y_ = gesture->delta_y();
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
  bool pinch_begin_;
  bool pinch_update_;
  bool pinch_end_;
  bool long_press_;
  bool fling_;
  bool three_finger_swipe_;

  gfx::Point scroll_begin_position_;

  float scroll_x_;
  float scroll_y_;
  float velocity_x_;
  float velocity_y_;
  int touch_id_;

  DISALLOW_COPY_AND_ASSIGN(GestureEventConsumeDelegate);
};

class QueueTouchEventDelegate : public GestureEventConsumeDelegate {
 public:
  explicit QueueTouchEventDelegate(RootWindow* root_window)
      : window_(NULL),
        root_window_(root_window) {
  }
  virtual ~QueueTouchEventDelegate() {}

  virtual ui::TouchStatus OnTouchEvent(TouchEvent* event) OVERRIDE {
    return event->type() == ui::ET_TOUCH_RELEASED ?
        ui::TOUCH_STATUS_QUEUED_END : ui::TOUCH_STATUS_QUEUED;
  }

  void ReceivedAck() {
    root_window_->AdvanceQueuedTouchEvent(window_, false);
  }

  void ReceivedAckPreventDefaulted() {
    root_window_->AdvanceQueuedTouchEvent(window_, true);
  }

  void set_window(Window* w) { window_ = w; }

 private:
  Window* window_;
  RootWindow* root_window_;

  DISALLOW_COPY_AND_ASSIGN(QueueTouchEventDelegate);
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
        mouse_move_(false),
        double_click_(false) {
  }

  void Reset() {
    mouse_enter_ = false;
    mouse_exit_ = false;
    mouse_press_ = false;
    mouse_release_ = false;
    mouse_move_ = false;
    double_click_ = false;
  }

  bool mouse_enter() const { return mouse_enter_; }
  bool mouse_exit() const { return mouse_exit_; }
  bool mouse_press() const { return mouse_press_; }
  bool mouse_move() const { return mouse_move_; }
  bool mouse_release() const { return mouse_release_; }
  bool double_click() const { return double_click_; }

  virtual bool OnMouseEvent(MouseEvent* event) OVERRIDE {
    switch (event->type()) {
      case ui::ET_MOUSE_PRESSED:
        double_click_ = event->flags() & ui::EF_IS_DOUBLE_CLICK;
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
  bool double_click_;

  DISALLOW_COPY_AND_ASSIGN(GestureEventSynthDelegate);
};

class TestOneShotGestureSequenceTimer
    : public base::OneShotTimer<ui::GestureSequence> {
 public:
  void ForceTimeout() {
    if (IsRunning()) {
      user_task().Run();
      Stop();
    }
  }
};

class TimerTestGestureSequence : public ui::GestureSequence {
 public:
  explicit TimerTestGestureSequence(ui::GestureEventHelper* helper)
      : ui::GestureSequence(helper) {
  }

  void ForceTimeout() {
    static_cast<TestOneShotGestureSequenceTimer*>(
        long_press_timer())->ForceTimeout();
  }

  base::OneShotTimer<ui::GestureSequence>* CreateTimer() {
    return new TestOneShotGestureSequenceTimer();
  }
};

class TestGestureRecognizer : public ui::GestureRecognizerImpl {
  public:
  explicit TestGestureRecognizer(RootWindow* root_window)
      : GestureRecognizerImpl(root_window) {
  }

  ui::GestureSequence* GetGestureSequenceForTesting(Window* window) {
    return GetGestureSequenceForConsumer(window);
  }
};

class TimerTestGestureRecognizer : public TestGestureRecognizer {
 public:
  explicit TimerTestGestureRecognizer(RootWindow* root_window)
      : TestGestureRecognizer(root_window) {
  }

  virtual ui::GestureSequence* CreateSequence(
      ui::GestureEventHelper* helper) OVERRIDE {
    return new TimerTestGestureSequence(helper);
  }
};

base::TimeDelta GetTime() {
  return base::Time::NowFromSystemTime() - base::Time();
}

void SendScrollEvents(RootWindow* root_window,
                      int x_start,
                      int y_start,
                      base::TimeDelta time_start,
                      int dx,
                      int dy,
                      int touch_id,
                      int time_step,
                      int num_steps,
                      GestureEventConsumeDelegate* delegate) {
  int x = x_start;
  int y = y_start;
  base::TimeDelta time = time_start;

  for (int i = 0; i < num_steps; i++) {
    x += dx;
    y += dy;
    time = time + base::TimeDelta::FromMilliseconds(time_step);
    TouchEvent move(ui::ET_TOUCH_MOVED, gfx::Point(x, y),
                    touch_id, time);
    root_window->DispatchTouchEvent(&move);
  }
}

void SendScrollEvent(RootWindow* root_window,
                     int x,
                     int y,
                     int touch_id,
                     GestureEventConsumeDelegate* delegate) {
  delegate->Reset();
  TouchEvent move(ui::ET_TOUCH_MOVED, gfx::Point(x, y),
                  touch_id, GetTime());
  root_window->DispatchTouchEvent(&move);
}

}  // namespace

typedef AuraTestBase GestureRecognizerTest;

// Check that appropriate touch events generate tap gesture events.
TEST_F(GestureRecognizerTest, GestureEventTap) {
  scoped_ptr<GestureEventConsumeDelegate> delegate(
      new GestureEventConsumeDelegate());
  const int kWindowWidth = 123;
  const int kWindowHeight = 45;
  const int kTouchId = 2;
  gfx::Rect bounds(100, 200, kWindowWidth, kWindowHeight);
  scoped_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, NULL));

  delegate->Reset();
  TouchEvent press(ui::ET_TOUCH_PRESSED, gfx::Point(101, 201),
                   kTouchId, GetTime());
  root_window()->DispatchTouchEvent(&press);
  EXPECT_FALSE(delegate->tap());
  EXPECT_TRUE(delegate->tap_down());
  EXPECT_FALSE(delegate->double_tap());
  EXPECT_FALSE(delegate->scroll_begin());
  EXPECT_FALSE(delegate->scroll_update());
  EXPECT_FALSE(delegate->scroll_end());
  EXPECT_FALSE(delegate->long_press());

  // Make sure there is enough delay before the touch is released so that it is
  // recognized as a tap.
  delegate->Reset();
  TouchEvent release(ui::ET_TOUCH_RELEASED, gfx::Point(101, 201),
                     kTouchId, press.time_stamp() +
                     base::TimeDelta::FromMilliseconds(50));

  root_window()->DispatchTouchEvent(&release);
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
  const int kTouchId = 5;
  gfx::Rect bounds(100, 200, kWindowWidth, kWindowHeight);
  scoped_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, NULL));

  delegate->Reset();
  TouchEvent press(ui::ET_TOUCH_PRESSED, gfx::Point(101, 201),
                   kTouchId, GetTime());
  root_window()->DispatchTouchEvent(&press);
  EXPECT_FALSE(delegate->tap());
  EXPECT_TRUE(delegate->tap_down());
  EXPECT_FALSE(delegate->double_tap());
  EXPECT_FALSE(delegate->scroll_begin());
  EXPECT_FALSE(delegate->scroll_update());
  EXPECT_FALSE(delegate->scroll_end());

  // Move the touch-point enough so that it is considered as a scroll. This
  // should generate both SCROLL_BEGIN and SCROLL_UPDATE gestures.
  // The first movement is diagonal, to ensure that we have a free scroll,
  // and not a rail scroll.
  SendScrollEvent(root_window(), 130, 230, kTouchId, delegate.get());
  EXPECT_FALSE(delegate->tap());
  EXPECT_FALSE(delegate->tap_down());
  EXPECT_FALSE(delegate->double_tap());
  EXPECT_TRUE(delegate->scroll_begin());
  EXPECT_TRUE(delegate->scroll_update());
  EXPECT_FALSE(delegate->scroll_end());
  EXPECT_EQ(29, delegate->scroll_x());
  EXPECT_EQ(29, delegate->scroll_y());
  EXPECT_EQ(gfx::Point(1, 1).ToString(),
            delegate->scroll_begin_position().ToString());

  // Move some more to generate a few more scroll updates.
  SendScrollEvent(root_window(), 110, 211, kTouchId, delegate.get());
  EXPECT_FALSE(delegate->tap());
  EXPECT_FALSE(delegate->tap_down());
  EXPECT_FALSE(delegate->double_tap());
  EXPECT_FALSE(delegate->scroll_begin());
  EXPECT_TRUE(delegate->scroll_update());
  EXPECT_FALSE(delegate->scroll_end());
  EXPECT_EQ(-20, delegate->scroll_x());
  EXPECT_EQ(-19, delegate->scroll_y());

  SendScrollEvent(root_window(), 140, 215, kTouchId, delegate.get());
  EXPECT_FALSE(delegate->tap());
  EXPECT_FALSE(delegate->tap_down());
  EXPECT_FALSE(delegate->double_tap());
  EXPECT_FALSE(delegate->scroll_begin());
  EXPECT_TRUE(delegate->scroll_update());
  EXPECT_FALSE(delegate->scroll_end());
  EXPECT_EQ(30, delegate->scroll_x());
  EXPECT_EQ(4, delegate->scroll_y());

  // Release the touch. This should end the scroll.
  delegate->Reset();
  TouchEvent release(ui::ET_TOUCH_RELEASED, gfx::Point(101, 201),
                     kTouchId, press.time_stamp() +
                     base::TimeDelta::FromMilliseconds(50));
  root_window()->DispatchTouchEvent(&release);
  EXPECT_FALSE(delegate->tap());
  EXPECT_FALSE(delegate->tap_down());
  EXPECT_FALSE(delegate->double_tap());
  EXPECT_FALSE(delegate->scroll_begin());
  EXPECT_FALSE(delegate->scroll_update());
  EXPECT_TRUE(delegate->scroll_end());
}

// Check Scroll End Events report correct velocities
// if the user was on a horizontal rail
TEST_F(GestureRecognizerTest, GestureEventHorizontalRailFling) {
  scoped_ptr<GestureEventConsumeDelegate> delegate(
      new GestureEventConsumeDelegate());
  const int kTouchId = 7;
  gfx::Rect bounds(0, 0, 1000, 1000);
  scoped_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, NULL));

  TouchEvent press(ui::ET_TOUCH_PRESSED, gfx::Point(0, 0),
                   kTouchId, GetTime());
  root_window()->DispatchTouchEvent(&press);

  // Move the touch-point horizontally enough that it is considered a
  // horizontal scroll.
  SendScrollEvent(root_window(), 20, 1, kTouchId, delegate.get());
  EXPECT_EQ(0, delegate->scroll_y());
  EXPECT_EQ(20, delegate->scroll_x());

  // Get a high x velocity, while still staying on the rail
  SendScrollEvents(root_window(), 1, 1, press.time_stamp(),
                   100, 10, kTouchId, 1,
                   ui::GestureConfiguration::points_buffered_for_velocity(),
                   delegate.get());

  delegate->Reset();
  TouchEvent release(ui::ET_TOUCH_RELEASED, gfx::Point(101, 201),
                     kTouchId, GetTime());
  root_window()->DispatchTouchEvent(&release);

  EXPECT_TRUE(delegate->scroll_end());
  EXPECT_EQ(100000, delegate->velocity_x());
  EXPECT_EQ(0, delegate->velocity_y());
}

// Check Scroll End Events report correct velocities
// if the user was on a vertical rail
TEST_F(GestureRecognizerTest, GestureEventVerticalRailFling) {
  scoped_ptr<GestureEventConsumeDelegate> delegate(
      new GestureEventConsumeDelegate());
  const int kTouchId = 7;
  gfx::Rect bounds(0, 0, 1000, 1000);
  scoped_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, NULL));

  TouchEvent press(ui::ET_TOUCH_PRESSED, gfx::Point(0, 0),
                   kTouchId, GetTime());
  root_window()->DispatchTouchEvent(&press);

  // Move the touch-point vertically enough that it is considered a
  // vertical scroll.
  SendScrollEvent(root_window(), 1, 20, kTouchId, delegate.get());
  EXPECT_EQ(20, delegate->scroll_y());
  EXPECT_EQ(0, delegate->scroll_x());

  // Get a high y velocity, while still staying on the rail
  SendScrollEvents(root_window(), 1, 1, press.time_stamp(),
                   10, 100, kTouchId, 1,
                   ui::GestureConfiguration::points_buffered_for_velocity(),
                   delegate.get());

  delegate->Reset();
  TouchEvent release(ui::ET_TOUCH_RELEASED, gfx::Point(101, 201),
                     kTouchId, GetTime());
  root_window()->DispatchTouchEvent(&release);

  EXPECT_TRUE(delegate->scroll_end());
  EXPECT_EQ(0, delegate->velocity_x());
  EXPECT_EQ(100000, delegate->velocity_y());
}

// Check Scroll End Events report correct velocities
// if the user is not on a rail
TEST_F(GestureRecognizerTest, GestureEventNonRailFling) {
  scoped_ptr<GestureEventConsumeDelegate> delegate(
      new GestureEventConsumeDelegate());
  const int kTouchId = 7;
  gfx::Rect bounds(0, 0, 1000, 1000);
  scoped_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, NULL));

  TouchEvent press(ui::ET_TOUCH_PRESSED, gfx::Point(0, 0),
                   kTouchId, GetTime());
  root_window()->DispatchTouchEvent(&press);

  // Move the touch-point such that a non-rail scroll begins
  SendScrollEvent(root_window(), 20, 20, kTouchId, delegate.get());
  EXPECT_EQ(20, delegate->scroll_y());
  EXPECT_EQ(20, delegate->scroll_x());

  SendScrollEvents(root_window(), 1, 1, press.time_stamp(),
                   10, 100, kTouchId, 1,
                   ui::GestureConfiguration::points_buffered_for_velocity(),
                   delegate.get());

  delegate->Reset();
  TouchEvent release(ui::ET_TOUCH_RELEASED, gfx::Point(101, 201),
                     kTouchId, GetTime());
  root_window()->DispatchTouchEvent(&release);

  EXPECT_TRUE(delegate->scroll_end());
  EXPECT_EQ(10000, delegate->velocity_x());
  EXPECT_EQ(100000, delegate->velocity_y());
}

// Check that appropriate touch events generate long press events
TEST_F(GestureRecognizerTest, GestureEventLongPress) {
  scoped_ptr<GestureEventConsumeDelegate> delegate(
      new GestureEventConsumeDelegate());
  const int kWindowWidth = 123;
  const int kWindowHeight = 45;
  const int kTouchId = 2;
  gfx::Rect bounds(100, 200, kWindowWidth, kWindowHeight);
  scoped_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, NULL));

  delegate->Reset();

  TimerTestGestureRecognizer* gesture_recognizer =
      new TimerTestGestureRecognizer(root_window());
  TimerTestGestureSequence* gesture_sequence =
      static_cast<TimerTestGestureSequence*>(
          gesture_recognizer->GetGestureSequenceForTesting(window.get()));

  root_window()->SetGestureRecognizerForTesting(gesture_recognizer);

  TouchEvent press1(ui::ET_TOUCH_PRESSED, gfx::Point(101, 201),
                    kTouchId, GetTime());
  root_window()->DispatchTouchEvent(&press1);
  EXPECT_TRUE(delegate->tap_down());

  // We haven't pressed long enough for a long press to occur
  EXPECT_FALSE(delegate->long_press());

  // Wait until the timer runs out
  gesture_sequence->ForceTimeout();
  EXPECT_TRUE(delegate->long_press());
  EXPECT_EQ(0, delegate->touch_id());

  delegate->Reset();
  TouchEvent release1(ui::ET_TOUCH_RELEASED, gfx::Point(101, 201),
                      kTouchId, GetTime());
  root_window()->DispatchTouchEvent(&release1);
  EXPECT_FALSE(delegate->long_press());
}

// Check that scrolling cancels a long press
TEST_F(GestureRecognizerTest, GestureEventLongPressCancelledByScroll) {
  scoped_ptr<GestureEventConsumeDelegate> delegate(
      new GestureEventConsumeDelegate());
  const int kWindowWidth = 123;
  const int kWindowHeight = 45;
  const int kTouchId = 6;
  gfx::Rect bounds(100, 200, kWindowWidth, kWindowHeight);
  scoped_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, NULL));

  delegate->Reset();

  TimerTestGestureRecognizer* gesture_recognizer =
      new TimerTestGestureRecognizer(root_window());
  TimerTestGestureSequence* gesture_sequence =
      static_cast<TimerTestGestureSequence*>(
          gesture_recognizer->GetGestureSequenceForTesting(window.get()));

  root_window()->SetGestureRecognizerForTesting(gesture_recognizer);

  TouchEvent press1(ui::ET_TOUCH_PRESSED, gfx::Point(101, 201),
                    kTouchId, GetTime());
  root_window()->DispatchTouchEvent(&press1);
  EXPECT_TRUE(delegate->tap_down());

  // We haven't pressed long enough for a long press to occur
  EXPECT_FALSE(delegate->long_press());

  // Scroll around, to cancel the long press
  SendScrollEvent(root_window(), 130, 230, kTouchId, delegate.get());
  // Wait until the timer runs out
  gesture_sequence->ForceTimeout();
  EXPECT_FALSE(delegate->long_press());

  delegate->Reset();
  TouchEvent release1(ui::ET_TOUCH_RELEASED, gfx::Point(101, 201),
                      kTouchId, GetTime());
  root_window()->DispatchTouchEvent(&release1);
  EXPECT_FALSE(delegate->long_press());
}

// Check that pinching cancels a long press
TEST_F(GestureRecognizerTest, GestureEventLongPressCancelledByPinch) {
  scoped_ptr<GestureEventConsumeDelegate> delegate(
      new GestureEventConsumeDelegate());
  const int kWindowWidth = 300;
  const int kWindowHeight = 400;
  const int kTouchId1 = 8;
  const int kTouchId2 = 2;
  gfx::Rect bounds(5, 5, kWindowWidth, kWindowHeight);
  scoped_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, NULL));

  TimerTestGestureRecognizer* gesture_recognizer =
      new TimerTestGestureRecognizer(root_window());
  TimerTestGestureSequence* gesture_sequence =
      static_cast<TimerTestGestureSequence*>(
          gesture_recognizer->GetGestureSequenceForTesting(window.get()));

  root_window()->SetGestureRecognizerForTesting(gesture_recognizer);

  delegate->Reset();
  TouchEvent press(ui::ET_TOUCH_PRESSED, gfx::Point(101, 201),
                   kTouchId1, GetTime());
  root_window()->DispatchTouchEvent(&press);
  EXPECT_TRUE(delegate->tap_down());

  // We haven't pressed long enough for a long press to occur
  EXPECT_FALSE(delegate->long_press());

  // Pinch, to cancel the long press
  delegate->Reset();
  TouchEvent press2(ui::ET_TOUCH_PRESSED, gfx::Point(10, 10),
                    kTouchId2, GetTime());
  root_window()->DispatchTouchEvent(&press2);
  EXPECT_TRUE(delegate->tap_down());
  EXPECT_TRUE(delegate->pinch_begin());

  // Wait until the timer runs out
  gesture_sequence->ForceTimeout();

  // No long press occurred
  EXPECT_FALSE(delegate->long_press());

  delegate->Reset();
  TouchEvent release1(ui::ET_TOUCH_RELEASED, gfx::Point(101, 201),
                      kTouchId1, GetTime());
  root_window()->DispatchTouchEvent(&release1);
  EXPECT_FALSE(delegate->long_press());
}

// Check that horizontal scroll gestures cause scrolls on horizontal rails.
// Also tests that horizontal rails can be broken.
TEST_F(GestureRecognizerTest, GestureEventHorizontalRailScroll) {
  scoped_ptr<GestureEventConsumeDelegate> delegate(
      new GestureEventConsumeDelegate());
  const int kTouchId = 7;
  gfx::Rect bounds(0, 0, 1000, 1000);
  scoped_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, NULL));

  TouchEvent press(ui::ET_TOUCH_PRESSED, gfx::Point(0, 0),
                   kTouchId, GetTime());
  root_window()->DispatchTouchEvent(&press);

  // Move the touch-point horizontally enough that it is considered a
  // horizontal scroll.
  SendScrollEvent(root_window(), 20, 1, kTouchId, delegate.get());
  EXPECT_EQ(0, delegate->scroll_y());
  EXPECT_EQ(20, delegate->scroll_x());

  SendScrollEvent(root_window(), 25, 6, kTouchId, delegate.get());
  EXPECT_TRUE(delegate->scroll_update());
  EXPECT_EQ(5, delegate->scroll_x());
  // y shouldn't change, as we're on a horizontal rail.
  EXPECT_EQ(0, delegate->scroll_y());

  // Send enough information that a velocity can be calculated for the gesture,
  // and we can break the rail
  SendScrollEvents(root_window(), 1, 1, press.time_stamp(),
                   1, 100, kTouchId, 1,
                   ui::GestureConfiguration::points_buffered_for_velocity(),
                   delegate.get());

  SendScrollEvent(root_window(), 0, 0, kTouchId, delegate.get());
  SendScrollEvent(root_window(), 5, 5, kTouchId, delegate.get());

  // The rail should be broken
  EXPECT_TRUE(delegate->scroll_update());
  EXPECT_EQ(5, delegate->scroll_x());
  EXPECT_EQ(5, delegate->scroll_y());
}

// Check that vertical scroll gestures cause scrolls on vertical rails.
// Also tests that vertical rails can be broken.
TEST_F(GestureRecognizerTest, GestureEventVerticalRailScroll) {
  scoped_ptr<GestureEventConsumeDelegate> delegate(
      new GestureEventConsumeDelegate());
  const int kTouchId = 7;
  gfx::Rect bounds(0, 0, 1000, 1000);
  scoped_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, NULL));

  TouchEvent press(ui::ET_TOUCH_PRESSED, gfx::Point(0, 0),
                   kTouchId, GetTime());
  root_window()->DispatchTouchEvent(&press);

  // Move the touch-point vertically enough that it is considered a
  // vertical scroll.
  SendScrollEvent(root_window(), 1, 20, kTouchId, delegate.get());
  EXPECT_EQ(0, delegate->scroll_x());
  EXPECT_EQ(20, delegate->scroll_y());

  SendScrollEvent(root_window(), 6, 25, kTouchId, delegate.get());
  EXPECT_TRUE(delegate->scroll_update());
  EXPECT_EQ(5, delegate->scroll_y());
  // x shouldn't change, as we're on a vertical rail.
  EXPECT_EQ(0, delegate->scroll_x());

  // Send enough information that a velocity can be calculated for the gesture,
  // and we can break the rail
  SendScrollEvents(root_window(), 1, 1, press.time_stamp(),
                   100, 1, kTouchId, 1,
                   ui::GestureConfiguration::points_buffered_for_velocity(),
                   delegate.get());

  SendScrollEvent(root_window(), 0, 0, kTouchId, delegate.get());
  SendScrollEvent(root_window(), 5, 5, kTouchId, delegate.get());

  // The rail should be broken
  EXPECT_TRUE(delegate->scroll_update());
  EXPECT_EQ(5, delegate->scroll_x());
  EXPECT_EQ(5, delegate->scroll_y());
}

TEST_F(GestureRecognizerTest, GestureTapFollowedByScroll) {
  // First, tap. Then, do a scroll using the same touch-id.
  scoped_ptr<GestureEventConsumeDelegate> delegate(
      new GestureEventConsumeDelegate());
  const int kWindowWidth = 123;
  const int kWindowHeight = 45;
  const int kTouchId = 3;
  gfx::Rect bounds(100, 200, kWindowWidth, kWindowHeight);
  scoped_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, NULL));

  delegate->Reset();
  TouchEvent press(ui::ET_TOUCH_PRESSED, gfx::Point(101, 201),
                   kTouchId, GetTime());
  root_window()->DispatchTouchEvent(&press);
  EXPECT_FALSE(delegate->tap());
  EXPECT_TRUE(delegate->tap_down());
  EXPECT_FALSE(delegate->double_tap());
  EXPECT_FALSE(delegate->scroll_begin());
  EXPECT_FALSE(delegate->scroll_update());
  EXPECT_FALSE(delegate->scroll_end());

  // Make sure there is enough delay before the touch is released so that it is
  // recognized as a tap.
  delegate->Reset();
  TouchEvent release(ui::ET_TOUCH_RELEASED, gfx::Point(101, 201),
                     kTouchId, press.time_stamp() +
                     base::TimeDelta::FromMilliseconds(50));
  root_window()->DispatchTouchEvent(&release);
  EXPECT_TRUE(delegate->tap());
  EXPECT_FALSE(delegate->tap_down());
  EXPECT_FALSE(delegate->double_tap());
  EXPECT_FALSE(delegate->scroll_begin());
  EXPECT_FALSE(delegate->scroll_update());
  EXPECT_FALSE(delegate->scroll_end());

  // Now, do a scroll gesture. Delay it sufficiently so that it doesn't trigger
  // a double-tap.
  delegate->Reset();
  TouchEvent press1(ui::ET_TOUCH_PRESSED, gfx::Point(101, 201),
                    kTouchId, release.time_stamp() +
                    base::TimeDelta::FromMilliseconds(1000));
  root_window()->DispatchTouchEvent(&press1);
  EXPECT_FALSE(delegate->tap());
  EXPECT_TRUE(delegate->tap_down());
  EXPECT_FALSE(delegate->double_tap());
  EXPECT_FALSE(delegate->scroll_begin());
  EXPECT_FALSE(delegate->scroll_update());
  EXPECT_FALSE(delegate->scroll_end());

  // Move the touch-point enough so that it is considered as a scroll. This
  // should generate both SCROLL_BEGIN and SCROLL_UPDATE gestures.
  // The first movement is diagonal, to ensure that we have a free scroll,
  // and not a rail scroll.
  delegate->Reset();
  TouchEvent move(ui::ET_TOUCH_MOVED, gfx::Point(130, 230),
                  kTouchId, GetTime());
  root_window()->DispatchTouchEvent(&move);
  EXPECT_FALSE(delegate->tap());
  EXPECT_FALSE(delegate->tap_down());
  EXPECT_FALSE(delegate->double_tap());
  EXPECT_TRUE(delegate->scroll_begin());
  EXPECT_TRUE(delegate->scroll_update());
  EXPECT_FALSE(delegate->scroll_end());
  EXPECT_EQ(29, delegate->scroll_x());
  EXPECT_EQ(29, delegate->scroll_y());

  // Move some more to generate a few more scroll updates.
  delegate->Reset();
  TouchEvent move1(ui::ET_TOUCH_MOVED, gfx::Point(110, 211),
                   kTouchId, GetTime());
  root_window()->DispatchTouchEvent(&move1);
  EXPECT_FALSE(delegate->tap());
  EXPECT_FALSE(delegate->tap_down());
  EXPECT_FALSE(delegate->double_tap());
  EXPECT_FALSE(delegate->scroll_begin());
  EXPECT_TRUE(delegate->scroll_update());
  EXPECT_FALSE(delegate->scroll_end());
  EXPECT_EQ(-20, delegate->scroll_x());
  EXPECT_EQ(-19, delegate->scroll_y());

  delegate->Reset();
  TouchEvent move2(ui::ET_TOUCH_MOVED, gfx::Point(140, 215),
                   kTouchId, GetTime());
  root_window()->DispatchTouchEvent(&move2);
  EXPECT_FALSE(delegate->tap());
  EXPECT_FALSE(delegate->tap_down());
  EXPECT_FALSE(delegate->double_tap());
  EXPECT_FALSE(delegate->scroll_begin());
  EXPECT_TRUE(delegate->scroll_update());
  EXPECT_FALSE(delegate->scroll_end());
  EXPECT_EQ(30, delegate->scroll_x());
  EXPECT_EQ(4, delegate->scroll_y());

  // Release the touch. This should end the scroll.
  delegate->Reset();
  TouchEvent release1(ui::ET_TOUCH_RELEASED, gfx::Point(101, 201),
                      kTouchId, GetTime());
  root_window()->DispatchTouchEvent(&release1);
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

  const int kTouchId = 4;

  // This press is required for the GestureRecognizer to associate a target
  // with kTouchId
  TouchEvent press(ui::ET_TOUCH_PRESSED, gfx::Point(30, 30),
                   kTouchId, GetTime());
  root_window()->DispatchTouchEvent(&press);

  delegate->Reset();
  GestureEvent tap(ui::ET_GESTURE_TAP, 20, 20, 0,
      base::Time::Now(), 0, 6, 1 << kTouchId);
  root_window()->DispatchGestureEvent(&tap);
  EXPECT_TRUE(delegate->mouse_enter());
  EXPECT_TRUE(delegate->mouse_press());
  EXPECT_TRUE(delegate->mouse_release());
  EXPECT_TRUE(delegate->mouse_exit());
  EXPECT_FALSE(delegate->double_click());

  delegate->Reset();
  GestureEvent tap2(ui::ET_GESTURE_DOUBLE_TAP, 20, 20, 0,
      base::Time::Now(), 0, 0, 1 << kTouchId);
  root_window()->DispatchGestureEvent(&tap2);
  EXPECT_TRUE(delegate->mouse_enter());
  EXPECT_TRUE(delegate->mouse_press());
  EXPECT_TRUE(delegate->mouse_release());
  EXPECT_TRUE(delegate->mouse_exit());
  EXPECT_TRUE(delegate->double_click());
}

TEST_F(GestureRecognizerTest, AsynchronousGestureRecognition) {
  scoped_ptr<QueueTouchEventDelegate> queued_delegate(
      new QueueTouchEventDelegate(root_window()));
  const int kWindowWidth = 123;
  const int kWindowHeight = 45;
  const int kTouchId1 = 6;
  const int kTouchId2 = 4;
  gfx::Rect bounds(100, 200, kWindowWidth, kWindowHeight);
  scoped_ptr<aura::Window> queue(CreateTestWindowWithDelegate(
      queued_delegate.get(), -1234, bounds, NULL));

  queued_delegate->set_window(queue.get());

  // Touch down on the window. This should not generate any gesture event.
  queued_delegate->Reset();
  TouchEvent press(ui::ET_TOUCH_PRESSED, gfx::Point(101, 201),
                   kTouchId1, GetTime());
  root_window()->DispatchTouchEvent(&press);
  EXPECT_FALSE(queued_delegate->tap());
  EXPECT_FALSE(queued_delegate->tap_down());
  EXPECT_FALSE(queued_delegate->double_tap());
  EXPECT_FALSE(queued_delegate->scroll_begin());
  EXPECT_FALSE(queued_delegate->scroll_update());
  EXPECT_FALSE(queued_delegate->scroll_end());

  // Introduce some delay before the touch is released so that it is recognized
  // as a tap. However, this still should not create any gesture events.
  queued_delegate->Reset();
  TouchEvent release(ui::ET_TOUCH_RELEASED, gfx::Point(101, 201),
                     kTouchId1, press.time_stamp() +
                     base::TimeDelta::FromMilliseconds(50));
  root_window()->DispatchTouchEvent(&release);
  EXPECT_FALSE(queued_delegate->tap());
  EXPECT_FALSE(queued_delegate->tap_down());
  EXPECT_FALSE(queued_delegate->double_tap());
  EXPECT_FALSE(queued_delegate->scroll_begin());
  EXPECT_FALSE(queued_delegate->scroll_update());
  EXPECT_FALSE(queued_delegate->scroll_end());

  // Create another window, and place a touch-down on it. This should create a
  // tap-down gesture.
  scoped_ptr<GestureEventConsumeDelegate> delegate(
      new GestureEventConsumeDelegate());
  scoped_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -2345, gfx::Rect(0, 0, 50, 50), NULL));
  delegate->Reset();
  TouchEvent press2(ui::ET_TOUCH_PRESSED, gfx::Point(10, 20),
                    kTouchId2, GetTime());
  root_window()->DispatchTouchEvent(&press2);
  EXPECT_FALSE(delegate->tap());
  EXPECT_TRUE(delegate->tap_down());
  EXPECT_FALSE(delegate->double_tap());
  EXPECT_FALSE(delegate->scroll_begin());
  EXPECT_FALSE(delegate->scroll_update());
  EXPECT_FALSE(delegate->scroll_end());

  TouchEvent release2(ui::ET_TOUCH_RELEASED, gfx::Point(10, 20),
                      kTouchId2, GetTime());
  root_window()->DispatchTouchEvent(&release2);

  // Process the first queued event.
  queued_delegate->Reset();
  queued_delegate->ReceivedAck();
  EXPECT_FALSE(queued_delegate->tap());
  EXPECT_TRUE(queued_delegate->tap_down());
  EXPECT_FALSE(queued_delegate->double_tap());
  EXPECT_FALSE(queued_delegate->scroll_begin());
  EXPECT_FALSE(queued_delegate->scroll_update());
  EXPECT_FALSE(queued_delegate->scroll_end());

  // Now, process the second queued event.
  queued_delegate->Reset();
  queued_delegate->ReceivedAck();
  EXPECT_TRUE(queued_delegate->tap());
  EXPECT_FALSE(queued_delegate->tap_down());
  EXPECT_FALSE(queued_delegate->double_tap());
  EXPECT_FALSE(queued_delegate->scroll_begin());
  EXPECT_FALSE(queued_delegate->scroll_update());
  EXPECT_FALSE(queued_delegate->scroll_end());

  // Start all over. Press on the first window, then press again on the second
  // window. The second press should still go to the first window.
  queued_delegate->Reset();
  TouchEvent press3(ui::ET_TOUCH_PRESSED, gfx::Point(101, 201),
                    kTouchId1, GetTime());
  root_window()->DispatchTouchEvent(&press3);
  EXPECT_FALSE(queued_delegate->tap());
  EXPECT_FALSE(queued_delegate->tap_down());
  EXPECT_FALSE(queued_delegate->double_tap());
  EXPECT_FALSE(queued_delegate->scroll_begin());
  EXPECT_FALSE(queued_delegate->scroll_update());
  EXPECT_FALSE(queued_delegate->scroll_end());

  queued_delegate->Reset();
  delegate->Reset();
  TouchEvent press4(ui::ET_TOUCH_PRESSED, gfx::Point(103, 203),
                    kTouchId2, GetTime());
  root_window()->DispatchTouchEvent(&press4);
  EXPECT_FALSE(delegate->tap());
  EXPECT_FALSE(delegate->tap_down());
  EXPECT_FALSE(delegate->double_tap());
  EXPECT_FALSE(delegate->scroll_begin());
  EXPECT_FALSE(delegate->scroll_update());
  EXPECT_FALSE(delegate->scroll_end());
  EXPECT_FALSE(queued_delegate->tap());
  EXPECT_FALSE(queued_delegate->tap_down());
  EXPECT_FALSE(queued_delegate->double_tap());
  EXPECT_FALSE(queued_delegate->scroll_begin());
  EXPECT_FALSE(queued_delegate->scroll_update());
  EXPECT_FALSE(queued_delegate->scroll_end());

  queued_delegate->Reset();
  queued_delegate->ReceivedAck();
  EXPECT_FALSE(queued_delegate->tap());
  EXPECT_TRUE(queued_delegate->tap_down());
  EXPECT_FALSE(queued_delegate->double_tap());
  EXPECT_FALSE(queued_delegate->scroll_begin());
  EXPECT_FALSE(queued_delegate->scroll_update());
  EXPECT_FALSE(queued_delegate->scroll_end());

  queued_delegate->Reset();
  queued_delegate->ReceivedAck();
  EXPECT_FALSE(queued_delegate->tap());
  EXPECT_TRUE(queued_delegate->tap_down());
  EXPECT_FALSE(queued_delegate->double_tap());
  EXPECT_TRUE(queued_delegate->scroll_begin());
  EXPECT_FALSE(queued_delegate->scroll_update());
  EXPECT_FALSE(queued_delegate->scroll_end());
  EXPECT_TRUE(queued_delegate->pinch_begin());
  EXPECT_FALSE(queued_delegate->pinch_update());
  EXPECT_FALSE(queued_delegate->pinch_end());
}

// Check that appropriate touch events generate pinch gesture events.
TEST_F(GestureRecognizerTest, GestureEventPinchFromScroll) {
  scoped_ptr<GestureEventConsumeDelegate> delegate(
      new GestureEventConsumeDelegate());
  const int kWindowWidth = 300;
  const int kWindowHeight = 400;
  const int kTouchId1 = 5;
  const int kTouchId2 = 3;
  gfx::Rect bounds(5, 5, kWindowWidth, kWindowHeight);
  scoped_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, NULL));

  aura::RootWindow* root = root_window();

  delegate->Reset();
  TouchEvent press(ui::ET_TOUCH_PRESSED, gfx::Point(101, 201),
                   kTouchId1, GetTime());
  root->DispatchTouchEvent(&press);
  EXPECT_FALSE(delegate->tap());
  EXPECT_TRUE(delegate->tap_down());
  EXPECT_FALSE(delegate->double_tap());
  EXPECT_FALSE(delegate->scroll_begin());
  EXPECT_FALSE(delegate->scroll_update());
  EXPECT_FALSE(delegate->scroll_end());

  // Move the touch-point enough so that it is considered as a scroll. This
  // should generate both SCROLL_BEGIN and SCROLL_UPDATE gestures.
  delegate->Reset();
  TouchEvent move(ui::ET_TOUCH_MOVED, gfx::Point(130, 201),
                  kTouchId1, GetTime());
  root->DispatchTouchEvent(&move);
  EXPECT_FALSE(delegate->tap());
  EXPECT_FALSE(delegate->tap_down());
  EXPECT_FALSE(delegate->double_tap());
  EXPECT_TRUE(delegate->scroll_begin());
  EXPECT_TRUE(delegate->scroll_update());
  EXPECT_FALSE(delegate->scroll_end());

  // Press the second finger. It should cause both a tap-down and pinch-begin.
  delegate->Reset();
  TouchEvent press2(ui::ET_TOUCH_PRESSED, gfx::Point(10, 10),
                    kTouchId2, GetTime());
  root->DispatchTouchEvent(&press2);
  EXPECT_FALSE(delegate->tap());
  EXPECT_TRUE(delegate->tap_down());
  EXPECT_FALSE(delegate->double_tap());
  EXPECT_FALSE(delegate->scroll_begin());
  EXPECT_FALSE(delegate->scroll_update());
  EXPECT_FALSE(delegate->scroll_end());
  EXPECT_TRUE(delegate->pinch_begin());

  // Move the first finger.
  delegate->Reset();
  TouchEvent move3(ui::ET_TOUCH_MOVED, gfx::Point(95, 201),
                   kTouchId1, GetTime());
  root->DispatchTouchEvent(&move3);
  EXPECT_FALSE(delegate->tap());
  EXPECT_FALSE(delegate->tap_down());
  EXPECT_FALSE(delegate->double_tap());
  EXPECT_FALSE(delegate->scroll_begin());
  EXPECT_FALSE(delegate->scroll_update());
  EXPECT_FALSE(delegate->scroll_end());
  EXPECT_FALSE(delegate->pinch_begin());
  EXPECT_TRUE(delegate->pinch_update());

  // Now move the second finger.
  delegate->Reset();
  TouchEvent move4(ui::ET_TOUCH_MOVED, gfx::Point(55, 15),
                   kTouchId2, GetTime());
  root->DispatchTouchEvent(&move4);
  EXPECT_FALSE(delegate->tap());
  EXPECT_FALSE(delegate->tap_down());
  EXPECT_FALSE(delegate->double_tap());
  EXPECT_FALSE(delegate->scroll_begin());
  EXPECT_FALSE(delegate->scroll_update());
  EXPECT_FALSE(delegate->scroll_end());
  EXPECT_FALSE(delegate->pinch_begin());
  EXPECT_TRUE(delegate->pinch_update());

  // Release the first finger. This should end pinch.
  delegate->Reset();
  TouchEvent release(ui::ET_TOUCH_RELEASED, gfx::Point(101, 201),
                     kTouchId1, press.time_stamp() +
                     base::TimeDelta::FromMilliseconds(50));
  root->DispatchTouchEvent(&release);
  EXPECT_FALSE(delegate->tap());
  EXPECT_FALSE(delegate->tap_down());
  EXPECT_FALSE(delegate->double_tap());
  EXPECT_FALSE(delegate->scroll_begin());
  EXPECT_FALSE(delegate->scroll_update());
  EXPECT_FALSE(delegate->scroll_end());
  EXPECT_TRUE(delegate->pinch_end());

  // Move the second finger. This should still generate a scroll.
  delegate->Reset();
  TouchEvent move5(ui::ET_TOUCH_MOVED, gfx::Point(25, 10),
                   kTouchId2, GetTime());
  root->DispatchTouchEvent(&move5);
  EXPECT_FALSE(delegate->tap());
  EXPECT_FALSE(delegate->tap_down());
  EXPECT_FALSE(delegate->double_tap());
  EXPECT_FALSE(delegate->scroll_begin());
  EXPECT_TRUE(delegate->scroll_update());
  EXPECT_FALSE(delegate->scroll_end());
  EXPECT_FALSE(delegate->pinch_begin());
  EXPECT_FALSE(delegate->pinch_update());
}

TEST_F(GestureRecognizerTest, GestureEventPinchFromScrollFromPinch) {
scoped_ptr<GestureEventConsumeDelegate> delegate(
      new GestureEventConsumeDelegate());
  const int kWindowWidth = 300;
  const int kWindowHeight = 400;
  const int kTouchId1 = 5;
  const int kTouchId2 = 3;
  gfx::Rect bounds(5, 5, kWindowWidth, kWindowHeight);
  scoped_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, NULL));

  TouchEvent press(ui::ET_TOUCH_PRESSED, gfx::Point(101, 201),
                   kTouchId1, GetTime());
  root_window()->DispatchTouchEvent(&press);
  delegate->Reset();
  TouchEvent press2(ui::ET_TOUCH_PRESSED, gfx::Point(10, 10),
                    kTouchId2, GetTime());
  root_window()->DispatchTouchEvent(&press2);
  EXPECT_TRUE(delegate->pinch_begin());

  SendScrollEvent(root_window(), 130, 230, kTouchId1, delegate.get());
  EXPECT_TRUE(delegate->pinch_update());

  // Pinch has started, now release the second finger
  delegate->Reset();
  TouchEvent release(ui::ET_TOUCH_RELEASED, gfx::Point(101, 201),
                     kTouchId1, GetTime());
  root_window()->DispatchTouchEvent(&release);
  EXPECT_TRUE(delegate->pinch_end());

  SendScrollEvent(root_window(), 130, 230, kTouchId2, delegate.get());
  EXPECT_TRUE(delegate->scroll_update());

  // Pinch again
  delegate->Reset();
  TouchEvent press3(ui::ET_TOUCH_PRESSED, gfx::Point(10, 10),
                    kTouchId1, GetTime());
  root_window()->DispatchTouchEvent(&press3);
  EXPECT_TRUE(delegate->pinch_begin());

  SendScrollEvent(root_window(), 130, 230, kTouchId1, delegate.get());
  EXPECT_TRUE(delegate->pinch_update());
}

TEST_F(GestureRecognizerTest, GestureEventPinchFromTap) {
  scoped_ptr<GestureEventConsumeDelegate> delegate(
      new GestureEventConsumeDelegate());
  const int kWindowWidth = 300;
  const int kWindowHeight = 400;
  const int kTouchId1 = 3;
  const int kTouchId2 = 5;
  gfx::Rect bounds(5, 5, kWindowWidth, kWindowHeight);
  scoped_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, NULL));

  aura::RootWindow* root = root_window();

  delegate->Reset();
  TouchEvent press(ui::ET_TOUCH_PRESSED, gfx::Point(101, 201),
                   kTouchId1, GetTime());
  root->DispatchTouchEvent(&press);
  EXPECT_FALSE(delegate->tap());
  EXPECT_TRUE(delegate->tap_down());
  EXPECT_FALSE(delegate->double_tap());
  EXPECT_FALSE(delegate->scroll_begin());
  EXPECT_FALSE(delegate->scroll_update());
  EXPECT_FALSE(delegate->scroll_end());

  // Press the second finger. It should cause a tap-down, scroll-begin and
  // pinch-begin.
  delegate->Reset();
  TouchEvent press2(ui::ET_TOUCH_PRESSED, gfx::Point(10, 10),
                    kTouchId2, GetTime());
  root->DispatchTouchEvent(&press2);
  EXPECT_FALSE(delegate->tap());
  EXPECT_TRUE(delegate->tap_down());
  EXPECT_FALSE(delegate->double_tap());
  EXPECT_TRUE(delegate->scroll_begin());
  EXPECT_FALSE(delegate->scroll_update());
  EXPECT_FALSE(delegate->scroll_end());
  EXPECT_TRUE(delegate->pinch_begin());

  // Move the first finger.
  delegate->Reset();
  TouchEvent move3(ui::ET_TOUCH_MOVED, gfx::Point(65, 201),
                   kTouchId1, GetTime());
  root->DispatchTouchEvent(&move3);
  EXPECT_FALSE(delegate->tap());
  EXPECT_FALSE(delegate->tap_down());
  EXPECT_FALSE(delegate->double_tap());
  EXPECT_FALSE(delegate->scroll_begin());
  EXPECT_FALSE(delegate->scroll_update());
  EXPECT_FALSE(delegate->scroll_end());
  EXPECT_FALSE(delegate->pinch_begin());
  EXPECT_TRUE(delegate->pinch_update());

  // Now move the second finger.
  delegate->Reset();
  TouchEvent move4(ui::ET_TOUCH_MOVED, gfx::Point(55, 15),
                   kTouchId2, GetTime());
  root->DispatchTouchEvent(&move4);
  EXPECT_FALSE(delegate->tap());
  EXPECT_FALSE(delegate->tap_down());
  EXPECT_FALSE(delegate->double_tap());
  EXPECT_FALSE(delegate->scroll_begin());
  EXPECT_FALSE(delegate->scroll_update());
  EXPECT_FALSE(delegate->scroll_end());
  EXPECT_FALSE(delegate->pinch_begin());
  EXPECT_TRUE(delegate->pinch_update());

  // Release the first finger. This should end pinch.
  delegate->Reset();
  TouchEvent release(ui::ET_TOUCH_RELEASED, gfx::Point(101, 201),
                     kTouchId1, press.time_stamp() +
                     base::TimeDelta::FromMilliseconds(50));
  root->DispatchTouchEvent(&release);
  EXPECT_FALSE(delegate->tap());
  EXPECT_FALSE(delegate->tap_down());
  EXPECT_FALSE(delegate->double_tap());
  EXPECT_FALSE(delegate->scroll_begin());
  EXPECT_FALSE(delegate->scroll_update());
  EXPECT_FALSE(delegate->scroll_end());
  EXPECT_TRUE(delegate->pinch_end());

  // Move the second finger. This should still generate a scroll.
  delegate->Reset();
  TouchEvent move5(ui::ET_TOUCH_MOVED, gfx::Point(25, 10),
                   kTouchId2, GetTime());
  root->DispatchTouchEvent(&move5);
  EXPECT_FALSE(delegate->tap());
  EXPECT_FALSE(delegate->tap_down());
  EXPECT_FALSE(delegate->double_tap());
  EXPECT_FALSE(delegate->scroll_begin());
  EXPECT_TRUE(delegate->scroll_update());
  EXPECT_FALSE(delegate->scroll_end());
  EXPECT_FALSE(delegate->pinch_begin());
  EXPECT_FALSE(delegate->pinch_update());
}

TEST_F(GestureRecognizerTest, GestureEventPinchScrollOnlyWhenBothFingersMove) {
  scoped_ptr<GestureEventConsumeDelegate> delegate(
      new GestureEventConsumeDelegate());
  const int kWindowWidth = 1000;
  const int kWindowHeight = 1000;
  const int kTouchId1 = 3;
  const int kTouchId2 = 5;
  gfx::Rect bounds(0, 0, kWindowWidth, kWindowHeight);
  scoped_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, NULL));

  delegate->Reset();
  TouchEvent press1(ui::ET_TOUCH_PRESSED, gfx::Point(100, 100),
                    kTouchId1, GetTime());

  root_window()->DispatchTouchEvent(&press1);

  delegate->Reset();
  TouchEvent press2(ui::ET_TOUCH_PRESSED, gfx::Point(110, 110),
                    kTouchId2, GetTime());
  root_window()->DispatchTouchEvent(&press2);

  SendScrollEvents(root_window(), 100, 100, press1.time_stamp(),
                   1, 10, kTouchId1, 1,
                   ui::GestureConfiguration::points_buffered_for_velocity(),
                   delegate.get());

  SendScrollEvents(root_window(), 110, 110, press1.time_stamp() +
                   base::TimeDelta::FromMilliseconds(1000),
                   1, 10, kTouchId2, 1,
                   ui::GestureConfiguration::points_buffered_for_velocity(),
                   delegate.get());
  // At no point were both fingers moving at the same time,
  // so no scrolling should have occurred

  EXPECT_EQ(0, delegate->scroll_x());
  EXPECT_EQ(0, delegate->scroll_y());
}

TEST_F(GestureRecognizerTest, GestureEventIgnoresDisconnectedEvents) {
  scoped_ptr<GestureEventConsumeDelegate> delegate(
      new GestureEventConsumeDelegate());

  TouchEvent release1(ui::ET_TOUCH_RELEASED, gfx::Point(101, 201),
                      6, GetTime());
  root_window()->DispatchTouchEvent(&release1);
  EXPECT_FALSE(delegate->tap());
  EXPECT_FALSE(delegate->tap_down());
}

// Check that a touch is locked to the window of the closest current touch
// within max_separation_for_gesture_touches_in_pixels
TEST_F(GestureRecognizerTest, GestureEventTouchLockSelectsCorrectWindow) {
  ui::GestureRecognizer* gesture_recognizer =
      new ui::GestureRecognizerImpl(root_window());
  root_window()->SetGestureRecognizerForTesting(gesture_recognizer);

  ui::GestureConsumer* target;
  const int kNumWindows = 4;

  scoped_array<GestureEventConsumeDelegate*> delegates(
      new GestureEventConsumeDelegate*[kNumWindows]);

  ui::GestureConfiguration::
      set_max_separation_for_gesture_touches_in_pixels(499);

  scoped_array<gfx::Rect*> window_bounds(new gfx::Rect*[kNumWindows]);
  window_bounds[0] = new gfx::Rect(0, 0, 1, 1);
  window_bounds[1] = new gfx::Rect(500, 0, 1, 1);
  window_bounds[2] = new gfx::Rect(0, 500, 1, 1);
  window_bounds[3] = new gfx::Rect(500, 500, 1, 1);

  scoped_array<aura::Window*> windows(new aura::Window*[kNumWindows]);

  // Instantiate windows with |window_bounds| and touch each window at
  // its origin.
  for (int i = 0; i < kNumWindows; ++i) {
    delegates[i] = new GestureEventConsumeDelegate();
    windows[i] = CreateTestWindowWithDelegate(
        delegates[i], i, *window_bounds[i], NULL);
    TouchEvent press(ui::ET_TOUCH_PRESSED, window_bounds[i]->origin(),
                     i, GetTime());
    root_window()->DispatchTouchEvent(&press);
  }

  // Touches should now be associated with the closest touch within
  // ui::GestureConfiguration::max_separation_for_gesture_touches_in_pixels
  target = gesture_recognizer->GetTargetForLocation(gfx::Point(11, 11));
  EXPECT_EQ(windows[0], target);
  target = gesture_recognizer->GetTargetForLocation(gfx::Point(511, 11));
  EXPECT_EQ(windows[1], target);
  target = gesture_recognizer->GetTargetForLocation(gfx::Point(11, 511));
  EXPECT_EQ(windows[2], target);
  target = gesture_recognizer->GetTargetForLocation(gfx::Point(511, 511));
  EXPECT_EQ(windows[3], target);

  // Add a touch in the middle associated with windows[2]
  TouchEvent press(ui::ET_TOUCH_PRESSED, gfx::Point(0, 500),
                   kNumWindows, GetTime());
  root_window()->DispatchTouchEvent(&press);
  TouchEvent move(ui::ET_TOUCH_MOVED, gfx::Point(250, 250),
                  kNumWindows, GetTime());
  root_window()->DispatchTouchEvent(&move);

  target = gesture_recognizer->GetTargetForLocation(gfx::Point(250, 250));
  EXPECT_EQ(windows[2], target);

  // Make sure that ties are broken by distance to a current touch
  // Closer to the point in the bottom right.
  target = gesture_recognizer->GetTargetForLocation(gfx::Point(380, 380));
  EXPECT_EQ(windows[3], target);

  // This touch is closer to the point in the middle
  target = gesture_recognizer->GetTargetForLocation(gfx::Point(300, 300));
  EXPECT_EQ(windows[2], target);

  // A touch too far from other touches won't be locked to anything
  target = gesture_recognizer->GetTargetForLocation(gfx::Point(1000, 1000));
  EXPECT_TRUE(target == NULL);

  // Move a touch associated with windows[2] to 1000, 1000
  TouchEvent move2(ui::ET_TOUCH_MOVED, gfx::Point(1000, 1000),
                   kNumWindows, GetTime());
  root_window()->DispatchTouchEvent(&move2);

  target = gesture_recognizer->GetTargetForLocation(gfx::Point(1000, 1000));
  EXPECT_EQ(windows[2], target);
}

// Check that touch events outside the root window are still handled
// by the root window's gesture sequence.
TEST_F(GestureRecognizerTest, GestureEventOutsideRootWindowTap) {
  TestGestureRecognizer* gesture_recognizer =
      new TestGestureRecognizer(root_window());
  root_window()->SetGestureRecognizerForTesting(gesture_recognizer);

  scoped_ptr<aura::Window> window(CreateTestWindowWithBounds(
      gfx::Rect(-100, -100, 2000, 2000), NULL));

  ui::GestureSequence* window_gesture_sequence =
      gesture_recognizer->GetGestureSequenceForTesting(window.get());

  ui::GestureSequence* root_window_gesture_sequence =
      gesture_recognizer->GetGestureSequenceForTesting(root_window());

  gfx::Point pos1(-10, -10);
  TouchEvent press1(ui::ET_TOUCH_PRESSED, pos1, 0, GetTime());
  root_window()->DispatchTouchEvent(&press1);

  gfx::Point pos2(1000, 1000);
  TouchEvent press2(ui::ET_TOUCH_PRESSED, pos2, 1, GetTime());
  root_window()->DispatchTouchEvent(&press2);

  // As these presses were outside the root window, they should be
  // associated with the root window.
  EXPECT_EQ(0, window_gesture_sequence->point_count());
  EXPECT_EQ(2, root_window_gesture_sequence->point_count());
}

TEST_F(GestureRecognizerTest, NoTapWithPreventDefaultedRelease) {
  scoped_ptr<QueueTouchEventDelegate> delegate(
      new QueueTouchEventDelegate(root_window()));
  const int kTouchId = 2;
  gfx::Rect bounds(100, 200, 100, 100);
  scoped_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, NULL));
  delegate->set_window(window.get());

  delegate->Reset();
  TouchEvent press(ui::ET_TOUCH_PRESSED, gfx::Point(101, 201),
                   kTouchId, GetTime());
  root_window()->DispatchTouchEvent(&press);
  TouchEvent release(ui::ET_TOUCH_RELEASED, gfx::Point(101, 201),
                     kTouchId, press.time_stamp() +
                     base::TimeDelta::FromMilliseconds(50));
  root_window()->DispatchTouchEvent(&release);

  delegate->Reset();
  delegate->ReceivedAck();
  EXPECT_TRUE(delegate->tap_down());
  delegate->Reset();
  delegate->ReceivedAckPreventDefaulted();
  EXPECT_FALSE(delegate->tap());
}

TEST_F(GestureRecognizerTest, GestureEventThreeFingerSwipe) {
  scoped_ptr<GestureEventConsumeDelegate> delegate(
      new GestureEventConsumeDelegate());
  const int kTouchId1 = 7;
  const int kTouchId2 = 2;
  const int kTouchId3 = 9;
  gfx::Rect bounds(0, 0, 1000, 1000);
  scoped_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -1234, bounds, NULL));

  TouchEvent press1(ui::ET_TOUCH_PRESSED, gfx::Point(0, 0),
                    kTouchId1, GetTime());
  TouchEvent press2(ui::ET_TOUCH_PRESSED, gfx::Point(0, 0),
                    kTouchId2, GetTime());
  TouchEvent press3(ui::ET_TOUCH_PRESSED, gfx::Point(0, 0),
                    kTouchId3, GetTime());

  TouchEvent release1(ui::ET_TOUCH_RELEASED, gfx::Point(0, 0),
                      kTouchId1, GetTime());
  TouchEvent release2(ui::ET_TOUCH_RELEASED, gfx::Point(0, 0),
                      kTouchId2, GetTime());
  TouchEvent release3(ui::ET_TOUCH_RELEASED, gfx::Point(0, 0),
                      kTouchId3, GetTime());

  root_window()->DispatchTouchEvent(&press1);
  root_window()->DispatchTouchEvent(&press2);
  root_window()->DispatchTouchEvent(&press3);

  delegate->Reset();

  // Start by testing all four directions

  int kBufferedPoints =
    ui::GestureConfiguration::points_buffered_for_velocity();

  // Swipe right
  SendScrollEvents(root_window(), 1, 1, press1.time_stamp(),
                   100, 10, kTouchId1, 1, kBufferedPoints, delegate.get());
  SendScrollEvents(root_window(), 1, 1, press2.time_stamp(),
                   100, 10, kTouchId2, 1, kBufferedPoints, delegate.get());
  SendScrollEvents(root_window(), 1, 1, press3.time_stamp(),
                   100, 10, kTouchId3, 1, kBufferedPoints, delegate.get());

  EXPECT_TRUE(delegate->three_finger_swipe());
  EXPECT_EQ(1, delegate->velocity_x());
  EXPECT_EQ(0, delegate->velocity_y());

  root_window()->DispatchTouchEvent(&release1);
  root_window()->DispatchTouchEvent(&release2);
  root_window()->DispatchTouchEvent(&release3);

  root_window()->DispatchTouchEvent(&press1);
  root_window()->DispatchTouchEvent(&press2);
  root_window()->DispatchTouchEvent(&press3);

  delegate->Reset();

  // Swipe left
  SendScrollEvents(root_window(), 1, 1,
                   press1.time_stamp(),
                   -100, 10, kTouchId1, 1, kBufferedPoints, delegate.get());
  SendScrollEvents(root_window(), 1, 1,
                   press2.time_stamp(),
                   -100, 10, kTouchId2, 1, kBufferedPoints, delegate.get());
  SendScrollEvents(root_window(), 1, 1,
                   press3.time_stamp(),
                   -100, 10, kTouchId3, 1, kBufferedPoints, delegate.get());

  EXPECT_TRUE(delegate->three_finger_swipe());
  EXPECT_EQ(-1, delegate->velocity_x());
  EXPECT_EQ(0, delegate->velocity_y());

  root_window()->DispatchTouchEvent(&release1);
  root_window()->DispatchTouchEvent(&release2);
  root_window()->DispatchTouchEvent(&release3);

  root_window()->DispatchTouchEvent(&press1);
  root_window()->DispatchTouchEvent(&press2);
  root_window()->DispatchTouchEvent(&press3);

  delegate->Reset();

  // Swipe down
  SendScrollEvents(root_window(), 1, 1,
                   press1.time_stamp(),
                   10, 100, kTouchId1, 1, kBufferedPoints, delegate.get());
  SendScrollEvents(root_window(), 1, 1,
                   press2.time_stamp(),
                   10, 100, kTouchId2, 1, kBufferedPoints, delegate.get());
  SendScrollEvents(root_window(), 1, 1,
                   press3.time_stamp(),
                   10, 100, kTouchId3, 1, kBufferedPoints, delegate.get());

  EXPECT_TRUE(delegate->three_finger_swipe());
  EXPECT_EQ(0, delegate->velocity_x());
  EXPECT_EQ(1, delegate->velocity_y());

  root_window()->DispatchTouchEvent(&release1);
  root_window()->DispatchTouchEvent(&release2);
  root_window()->DispatchTouchEvent(&release3);

  root_window()->DispatchTouchEvent(&press1);
  root_window()->DispatchTouchEvent(&press2);
  root_window()->DispatchTouchEvent(&press3);

  delegate->Reset();

  // Swipe up
  SendScrollEvents(root_window(), 1, 1,
                   press1.time_stamp(),
                   10, -100, kTouchId1, 1, kBufferedPoints, delegate.get());
  SendScrollEvents(root_window(), 1, 1,
                   press2.time_stamp(),
                   10, -100, kTouchId2, 1, kBufferedPoints, delegate.get());
  SendScrollEvents(root_window(), 1, 1,
                   press3.time_stamp(),
                   10, -100, kTouchId3, 1, kBufferedPoints, delegate.get());

  EXPECT_TRUE(delegate->three_finger_swipe());
  EXPECT_EQ(0, delegate->velocity_x());
  EXPECT_EQ(-1, delegate->velocity_y());

  // Only one swipe can occur per press of three fingers
  delegate->Reset();

  SendScrollEvents(root_window(), 1, 1,
                   press1.time_stamp(),
                   10, -100, kTouchId1, 1, kBufferedPoints, delegate.get());
  SendScrollEvents(root_window(), 1, 1,
                   press2.time_stamp(),
                   10, -100, kTouchId2, 1, kBufferedPoints, delegate.get());
  SendScrollEvents(root_window(), 1, 1,
                   press3.time_stamp(),
                   10, -100, kTouchId3, 1, kBufferedPoints, delegate.get());

  EXPECT_FALSE(delegate->three_finger_swipe());

  root_window()->DispatchTouchEvent(&release1);
  root_window()->DispatchTouchEvent(&release2);
  root_window()->DispatchTouchEvent(&release3);

  root_window()->DispatchTouchEvent(&press1);
  root_window()->DispatchTouchEvent(&press2);
  root_window()->DispatchTouchEvent(&press3);

  delegate->Reset();

  // Swiping diagonally doesn't fire an event
  SendScrollEvents(root_window(), 1, 1,
                   press1.time_stamp(),
                   100, -100, kTouchId1, 1, kBufferedPoints, delegate.get());
  SendScrollEvents(root_window(), 1, 1,
                   press2.time_stamp(),
                   100, -100, kTouchId2, 1, kBufferedPoints, delegate.get());
  SendScrollEvents(root_window(), 1, 1,
                   press3.time_stamp(),
                   100, -100, kTouchId3, 1, kBufferedPoints, delegate.get());

  EXPECT_FALSE(delegate->three_finger_swipe());

  root_window()->DispatchTouchEvent(&release1);
  root_window()->DispatchTouchEvent(&release2);
  root_window()->DispatchTouchEvent(&release3);

  root_window()->DispatchTouchEvent(&press1);
  root_window()->DispatchTouchEvent(&press2);
  root_window()->DispatchTouchEvent(&press3);

  delegate->Reset();

  // Have to swipe in a consistent direction
  SendScrollEvents(root_window(), 1, 1,
                   press1.time_stamp(),
                   100, 10, kTouchId1, 1, kBufferedPoints, delegate.get());
  SendScrollEvents(root_window(), 1, 1,
                   press2.time_stamp(),
                   100, 10, kTouchId2, 1, kBufferedPoints, delegate.get());
  SendScrollEvents(root_window(), 1, 1,
                   press3.time_stamp(),
                   -100, 10, kTouchId3, 1, kBufferedPoints, delegate.get());

  EXPECT_FALSE(delegate->three_finger_swipe());
}

}  // namespace test
}  // namespace aura

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
  GestureEventConsumeDelegate() {
    event_types_.reset(new std::vector<ui::EventType>());
    Reset();
  }

  virtual ~GestureEventConsumeDelegate() {}

  virtual void Reset() {
    event_types_->clear();
  }

  bool EventTypesAre(size_t num, ...) {
    if (num != event_types_->size())
      return false;

    va_list types;
    va_start(types, num);

    for (size_t i = 0; i < num; ++i) {
      if ((*event_types_.get())[i] != va_arg(types, int)) {
        va_end(types);
        return false;
      }
    }
    va_end(types);
    return true;
  }

  std::vector<ui::EventType>* event_types() {
    return event_types_.get();
  }

  virtual ui::GestureStatus OnGestureEvent(GestureEvent* gesture) OVERRIDE {
    event_types_->push_back(gesture->type());
    return ui::GESTURE_STATUS_CONSUMED;
  }

 protected:
  scoped_ptr<std::vector<ui::EventType> > event_types_;

 private:
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

class GestureEventSynthDelegate : public GestureEventConsumeDelegate {
 public:
  GestureEventSynthDelegate() {
  }

  virtual ui::GestureStatus OnGestureEvent(GestureEvent* gesture) OVERRIDE {
    return ui::GESTURE_STATUS_UNKNOWN;
  }

  bool double_click() const { return double_click_; }

  virtual bool OnMouseEvent(MouseEvent* event) OVERRIDE {
    if (event->type() == ui::ET_MOUSE_PRESSED &&
       event->flags() & ui::EF_IS_DOUBLE_CLICK)
      double_click_ = true;
    event_types_->push_back(event->type());
    return true;
  }

  virtual void Reset() {
    GestureEventConsumeDelegate::Reset();
    double_click_ = false;
  }

 private:
  bool double_click_;

  DISALLOW_COPY_AND_ASSIGN(GestureEventSynthDelegate);
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

base::TimeDelta GetTime() {
  return base::Time::NowFromSystemTime() - base::Time();
}

}  // namespace

typedef AuraTestBase GestureRecognizerTest;

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

  EXPECT_TRUE(delegate->EventTypesAre(4,
                                      ui::ET_MOUSE_ENTERED,
                                      ui::ET_MOUSE_PRESSED,
                                      ui::ET_MOUSE_RELEASED,
                                      ui::ET_MOUSE_EXITED));
  EXPECT_FALSE(delegate->double_click());
  delegate->Reset();

  GestureEvent tap2(ui::ET_GESTURE_DOUBLE_TAP, 20, 20, 0,
      base::Time::Now(), 0, 0, 1 << kTouchId);
  root_window()->DispatchGestureEvent(&tap2);

  EXPECT_TRUE(delegate->EventTypesAre(4,
                                      ui::ET_MOUSE_ENTERED,
                                      ui::ET_MOUSE_PRESSED,
                                      ui::ET_MOUSE_RELEASED,
                                      ui::ET_MOUSE_EXITED));
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
  TouchEvent press(ui::ET_TOUCH_PRESSED, gfx::Point(101, 201),
                   kTouchId1, GetTime());
  root_window()->DispatchTouchEvent(&press);

  EXPECT_TRUE(queued_delegate->EventTypesAre(0));
  queued_delegate->Reset();

  // Introduce some delay before the touch is released so that it is recognized
  // as a tap. However, this still should not create any gesture events.
  TouchEvent release(ui::ET_TOUCH_RELEASED, gfx::Point(101, 201),
                     kTouchId1, press.time_stamp() +
                     base::TimeDelta::FromMilliseconds(50));
  root_window()->DispatchTouchEvent(&release);

  EXPECT_TRUE(queued_delegate->EventTypesAre(0));
  queued_delegate->Reset();

  // Create another window, and place a touch-down on it. This should create a
  // tap-down gesture.
  scoped_ptr<GestureEventConsumeDelegate> delegate(
      new GestureEventConsumeDelegate());
  scoped_ptr<aura::Window> window(CreateTestWindowWithDelegate(
      delegate.get(), -2345, gfx::Rect(0, 0, 50, 50), NULL));
  TouchEvent press2(ui::ET_TOUCH_PRESSED, gfx::Point(10, 20),
                    kTouchId2, GetTime());

  root_window()->DispatchTouchEvent(&press2);

  EXPECT_TRUE(delegate->EventTypesAre(1, ui::ET_GESTURE_TAP_DOWN));
  delegate->Reset();

  TouchEvent release2(ui::ET_TOUCH_RELEASED, gfx::Point(10, 20),
                      kTouchId2, GetTime());
  root_window()->DispatchTouchEvent(&release2);

  // Process the first queued event.
  queued_delegate->ReceivedAck();
  EXPECT_TRUE(queued_delegate->EventTypesAre(1, ui::ET_GESTURE_TAP_DOWN));
  queued_delegate->Reset();

  // Now, process the second queued event.
  queued_delegate->ReceivedAck();
  EXPECT_TRUE(queued_delegate->EventTypesAre(1, ui::ET_GESTURE_TAP));
  queued_delegate->Reset();

  // Start all over. Press on the first window, then press again on the second
  // window. The second press should still go to the first window.
  TouchEvent press3(ui::ET_TOUCH_PRESSED, gfx::Point(101, 201),
                    kTouchId1, GetTime());
  root_window()->DispatchTouchEvent(&press3);
  EXPECT_TRUE(delegate->EventTypesAre(0));
  queued_delegate->Reset();
  delegate->Reset();
  TouchEvent press4(ui::ET_TOUCH_PRESSED, gfx::Point(103, 203),
                    kTouchId2, GetTime());
  root_window()->DispatchTouchEvent(&press4);
  EXPECT_TRUE(delegate->EventTypesAre(0));
  delegate->Reset();

  EXPECT_TRUE(queued_delegate->EventTypesAre(0));
  queued_delegate->Reset();

  queued_delegate->ReceivedAck();
  EXPECT_TRUE(queued_delegate->EventTypesAre(1, ui::ET_GESTURE_TAP_DOWN));
  queued_delegate->Reset();

  queued_delegate->ReceivedAck();
  EXPECT_TRUE(queued_delegate->EventTypesAre(3,
                                             ui::ET_GESTURE_TAP_DOWN,
                                             ui::ET_GESTURE_PINCH_BEGIN,
                                             ui::ET_GESTURE_SCROLL_BEGIN));
}

// Check that a touch is locked to the window of the closest current touch
// within max_separation_for_gesture_touches_in_pixels
TEST_F(GestureRecognizerTest, GestureEventTouchLockSelectsCorrectWindow) {
  ui::GestureRecognizer* gesture_recognizer =
      new ui::GestureRecognizerImpl(root_window());
  root_window()->SetGestureRecognizerForTesting(gesture_recognizer);

  ui::GestureConsumer* target;
  const int kNumWindows = 4;

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
    windows[i] = CreateTestWindowWithDelegate(
        new TestWindowDelegate(), i, *window_bounds[i], NULL);
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
  EXPECT_TRUE(delegate->EventTypesAre(1,
                                      ui::ET_GESTURE_TAP_DOWN));
  delegate->Reset();
  delegate->ReceivedAckPreventDefaulted();
  // No tap fired.
  EXPECT_TRUE(delegate->EventTypesAre(0));
}

}  // namespace test
}  // namespace aura

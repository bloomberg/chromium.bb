// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_EVENT_GENERATOR_H_
#define UI_AURA_TEST_EVENT_GENERATOR_H_
#pragma once

#include "base/basictypes.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/gfx/point.h"

namespace base {
class TimeDelta;
}

namespace aura {
class Event;
class RootWindow;
class Window;

namespace test {

// EventGenerator is a tool that generates and dispatch events.
class EventGenerator {
 public:
  // Creates an EventGenerator with the mouse/touch location (0,0).
  explicit EventGenerator(RootWindow* root_window);

  // Creates an EventGenerator with the mouse/touch location
  // at |initial_location|.
  EventGenerator(RootWindow* root_window, const gfx::Point& initial_location);

  // Creates an EventGenerator with the mouse/touch location
  // centered over |window|.
  EventGenerator(RootWindow* root_window, Window* window);

  virtual ~EventGenerator();

  const gfx::Point& current_location() const { return current_location_; }

  // Resets the event flags bitmask.
  void set_flags(int flags) { flags_ = flags; }

  // Generates a left button press event.
  void PressLeftButton();

  // Generates a left button release event.
  void ReleaseLeftButton();

  // Generates events to click (press, release) left button.
  void ClickLeftButton();

  // Generates a double click event using the left button.
  void DoubleClickLeftButton();

  // Generates events to move mouse to be the given |point|.
  void MoveMouseTo(const gfx::Point& point, int count);
  void MoveMouseTo(const gfx::Point& point) {
    MoveMouseTo(point, 1);
  }
  void MoveMouseTo(int x, int y) {
    MoveMouseTo(gfx::Point(x, y));
  }

  // Generates events to move mouse to be the given |point| in |window|'s
  // coordinates.
  void MoveMouseRelativeTo(const Window* window, const gfx::Point& point);
  void MoveMouseRelativeTo(const Window* window, int x, int y) {
    MoveMouseRelativeTo(window, gfx::Point(x, y));
  }

  void MoveMouseBy(int x, int y) {
    MoveMouseTo(current_location_.Add(gfx::Point(x, y)));
  }

  // Generates events to drag mouse to given |point|.
  void DragMouseTo(const gfx::Point& point);

  void DragMouseTo(int x, int y) {
    DragMouseTo(gfx::Point(x, y));
  }

  void DragMouseBy(int dx, int dy) {
    DragMouseTo(current_location_.Add(gfx::Point(dx, dy)));
  }

  // Generates events to move the mouse to the center of the window.
  void MoveMouseToCenterOf(Window* window);

  // Generates a touch press event.
  void PressTouch();

  // Generates a touch release event.
  void ReleaseTouch();

  // Generates press, move and release event to move touch
  // to be the given |point|.
  void PressMoveAndReleaseTouchTo(const gfx::Point& point);

  // Generates a number of touch-move events to generate scroll-gesture events.
  void SendTouchScrollEvents(const gfx::Point& start_location,
                             const base::TimeDelta& start_time,
                             int dx,
                             int dy,
                             int touch_id,
                             int time_step_ms,
                             int num_steps);

  void PressMoveAndReleaseTouchTo(int x, int y) {
    PressMoveAndReleaseTouchTo(gfx::Point(x, y));
  }

  void PressMoveAndReleaseTouchBy(int x, int y) {
    PressMoveAndReleaseTouchTo(current_location_.Add(gfx::Point(x, y)));
  }

  // Generates press, move and release events to move touch
  // to the center of the window.
  void PressMoveAndReleaseTouchToCenterOf(Window* window);

  // Generates and dispatches touch-events required to generate a TAP gesture.
  // Note that this can generate a number of other gesture events at the same
  // time (e.g. GESTURE_BEGIN, TAP_DOWN, END).
  void GestureTapAt(const gfx::Point& point);

  // Generates press and release touch-events to generate a TAP_DOWN event, but
  // without generating any scroll or tap events. This can also generate a few
  // other gesture events (e.g. GESTURE_BEGIN, END).
  void GestureTapDownAndUp(const gfx::Point& point);

  // Generates press, move, release touch-events to generate a sequence of
  // scroll events.
  void GestureScrollSequence(const gfx::Point& start,
                             const gfx::Point& end,
                             const base::TimeDelta& duration,
                             int steps);

  // Generates a key press event. On platforms except Windows and X11, a key
  // event without native_event() is generated. Note that ui::EF_ flags should
  // be passed as |flags|, not the native ones like 'ShiftMask' in <X11/X.h>.
  // TODO(yusukes): Support native_event() on all platforms.
  void PressKey(ui::KeyboardCode key_code, int flags);

  // Generates a key release event. On platforms except Windows and X11, a key
  // event without native_event() is generated. Note that ui::EF_ flags should
  // be passed as |flags|, not the native ones like 'ShiftMask' in <X11/X.h>.
  // TODO(yusukes): Support native_event() on all platforms.
  void ReleaseKey(ui::KeyboardCode key_code, int flags);

  // Dispatch the |event| to the RootWindow.
  void Dispatch(Event& event);

 private:
  // Dispatch a key event to the RootWindow.
  void DispatchKeyEvent(bool is_press, ui::KeyboardCode key_code, int flags);

  RootWindow* root_window_;
  int flags_;
  gfx::Point current_location_;

  DISALLOW_COPY_AND_ASSIGN(EventGenerator);
};

}  // namespace test
}  // namespace aura

#endif  // UI_AURA_TEST_EVENT_GENERATOR_H_

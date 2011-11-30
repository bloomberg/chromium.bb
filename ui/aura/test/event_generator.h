// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_EVENT_GENERATOR_H_
#define UI_AURA_TEST_EVENT_GENERATOR_H_
#pragma once

#include "base/basictypes.h"
#include "ui/gfx/point.h"

namespace aura {
class Event;
class Window;

namespace test {

// EventGenerator is a tool that generates and dispatch events.
// TODO(oshima): Support key events.
class EventGenerator {
 public:
  // Creates an EventGenerator with the mouse location (0,0).
  EventGenerator();

  // Creates an EventGenerator with the mouse location at |initial_location|.
  explicit EventGenerator(const gfx::Point& initial_location);

  // Creates an EventGenerator with the mouse location centered over |window|.
  explicit EventGenerator(Window* window);

  virtual ~EventGenerator();

  const gfx::Point& current_location() const { return current_location_; }

  // Generates a left button press event.
  void PressLeftButton();

  // Generates a left button release event.
  void ReleaseLeftButton();

  // Generates events to click (press, release) left button.
  void ClickLeftButton();

  // Generates a double click event using the left button.
  void DoubleClickLeftButton();

  // Generates events to move mouse to be the given |point|.
  void MoveMouseTo(const gfx::Point& point);

  void MoveMouseTo(int x, int y) {
    MoveMouseTo(gfx::Point(x, y));
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

 private:
  // Dispatch the |event| to the Desktop.
  void Dispatch(Event& event);

  int flags_;
  gfx::Point current_location_;

  DISALLOW_COPY_AND_ASSIGN(EventGenerator);
};

}  // namespace test
}  // namespace aura

#endif  // UI_AURA_TEST_EVENT_GENERATOR_H_

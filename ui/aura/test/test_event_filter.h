// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_TEST_EVENT_FILTER_H_
#define UI_AURA_TEST_TEST_EVENT_FILTER_H_

#include "base/compiler_specific.h"
#include "ui/aura/event_filter.h"

namespace aura {

class Window;

namespace test {

// TestEventFilter counts the key, mouse and touch events it receives and can
// optinally be set to consume those events.
class TestEventFilter : public EventFilter {
 public:
  TestEventFilter();
  virtual ~TestEventFilter();

  // Resets all event counters.
  void ResetCounts();

  int key_event_count() const { return key_event_count_; }
  int mouse_event_count() const { return mouse_event_count_; }
  int touch_event_count() const { return touch_event_count_; }

  void set_key_event_handling_result(ui::EventResult result) {
    key_event_handling_result_ = result;
  }
  void set_mouse_event_handling_result(ui::EventResult result) {
    mouse_event_handling_result_ = result;
  }
  void set_consumes_touch_events(bool consumes) {
    consumes_touch_event_ = consumes;
  }

  // Overridden from ui::EventHandler:
  virtual ui::EventResult OnKeyEvent(ui::KeyEvent* event) OVERRIDE;
  virtual ui::EventResult OnMouseEvent(ui::MouseEvent* event) OVERRIDE;
  virtual ui::EventResult OnScrollEvent(ui::ScrollEvent* event) OVERRIDE;
  virtual ui::TouchStatus OnTouchEvent(ui::TouchEvent* event) OVERRIDE;
  virtual ui::EventResult OnGestureEvent(ui::GestureEvent* event) OVERRIDE;
 private:
  int key_event_count_;
  int mouse_event_count_;
  int touch_event_count_;

  ui::EventResult key_event_handling_result_;
  ui::EventResult mouse_event_handling_result_;
  bool consumes_touch_event_;

  DISALLOW_COPY_AND_ASSIGN(TestEventFilter);
};

}  // namespace test
}  // namespace aura

#endif  // UI_AURA_TEST_TEST_EVENT_FILTER_H_

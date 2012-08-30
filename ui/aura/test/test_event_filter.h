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
  int post_key_event_count() const { return post_key_event_count_; }
  int post_mouse_event_count() const { return post_mouse_event_count_; }
  int post_touch_event_count() const { return post_touch_event_count_; }

  void set_consumes_key_events(bool consumes_key_events) {
    consumes_key_events_ = consumes_key_events;
  }
  void set_consumes_mouse_events(bool consumes_mouse_events) {
    consumes_mouse_events_ = consumes_mouse_events;
  }
  void set_consumes_touch_events(bool consumes_touch_events) {
    consumes_touch_events_ = consumes_touch_events;
  }
  void set_consumes_key_events_post(bool consumes_key_events_post) {
    consumes_key_events_post_ = consumes_key_events_post;
  }
  void set_consumes_mouse_events_post(bool consumes_mouse_events_post) {
    consumes_mouse_events_post_ = consumes_mouse_events_post;
  }
  void set_consumes_touch_events_post(bool consumes_touch_events_post) {
    consumes_touch_events_post_ = consumes_touch_events_post;
  }

  // Overridden from EventFilter:
  virtual bool PreHandleKeyEvent(Window* target, ui::KeyEvent* event) OVERRIDE;
  virtual bool PreHandleMouseEvent(Window* target,
                                   ui::MouseEvent* event) OVERRIDE;
  virtual ui::TouchStatus PreHandleTouchEvent(
      Window* target,
      ui::TouchEvent* event) OVERRIDE;
  virtual ui::GestureStatus PreHandleGestureEvent(
      Window* target,
      ui::GestureEvent* event) OVERRIDE;
  virtual bool PostHandleKeyEvent(Window* target, ui::KeyEvent* event) OVERRIDE;
  virtual bool PostHandleMouseEvent(Window* target,
                                    ui::MouseEvent* event) OVERRIDE;
  virtual ui::TouchStatus PostHandleTouchEvent(Window* target,
                                               ui::TouchEvent* event) OVERRIDE;
  virtual ui::GestureStatus PostHandleGestureEvent(
      Window* target,
      ui::GestureEvent* event) OVERRIDE;

 private:
  int key_event_count_;
  int mouse_event_count_;
  int touch_event_count_;
  int post_key_event_count_;
  int post_mouse_event_count_;
  int post_touch_event_count_;

  bool consumes_key_events_;
  bool consumes_mouse_events_;
  bool consumes_touch_events_;
  bool consumes_key_events_post_;
  bool consumes_mouse_events_post_;
  bool consumes_touch_events_post_;

  DISALLOW_COPY_AND_ASSIGN(TestEventFilter);
};

}  // namespace test
}  // namespace aura

#endif  // UI_AURA_TEST_TEST_EVENT_FILTER_H_

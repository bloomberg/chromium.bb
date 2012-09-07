// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/event_filter.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/aura/focus_manager.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/test/test_event_filter.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/base/events/event.h"

#if defined(OS_WIN)
// Windows headers define macros for these function names which screw with us.
#if defined(CreateWindow)
#undef CreateWindow
#endif
#endif

namespace aura {

typedef test::AuraTestBase EventFilterTest;

class TestEventFilterWindowDelegate : public test::TestWindowDelegate {
 public:
  TestEventFilterWindowDelegate()
      : key_event_count_(0),
        mouse_event_count_(0),
        touch_event_count_(0),
        key_event_handling_result_(ui::ER_UNHANDLED),
        mouse_event_handling_result_(ui::ER_UNHANDLED),
        consumes_touch_events_(true) {}
  virtual ~TestEventFilterWindowDelegate() {}

  void ResetCounts() {
    key_event_count_ = 0;
    mouse_event_count_ = 0;
    touch_event_count_ = 0;
  }

  int key_event_count() const { return key_event_count_; }
  int mouse_event_count() const { return mouse_event_count_; }
  int touch_event_count() const { return touch_event_count_; }

  void set_key_event_handling_result(ui::EventResult result) {
    key_event_handling_result_ = result;
  }
  void set_mouse_event_handling_result(ui::EventResult result) {
    mouse_event_handling_result_ = result;
  }
  void set_consumes_touch_events(bool consumes_touch_events) {
    consumes_touch_events_ = consumes_touch_events;
  }

  // Overridden from EventHandler:
  virtual ui::EventResult OnKeyEvent(ui::KeyEvent* event) OVERRIDE {
    ++key_event_count_;
    return key_event_handling_result_;
  }

  virtual ui::EventResult OnMouseEvent(ui::MouseEvent* event) OVERRIDE {
    ++mouse_event_count_;
    return mouse_event_handling_result_;
  }

  virtual ui::TouchStatus OnTouchEvent(ui::TouchEvent* event) OVERRIDE {
    ++touch_event_count_;
    // TODO(sadrul): !
    return ui::TOUCH_STATUS_UNKNOWN;
  }

  virtual ui::EventResult OnGestureEvent(ui::GestureEvent* event) OVERRIDE {
    // TODO(sadrul): !
    return ui::ER_UNHANDLED;
  }
 private:
  int key_event_count_;
  int mouse_event_count_;
  int touch_event_count_;
  ui::EventResult key_event_handling_result_;
  ui::EventResult mouse_event_handling_result_;
  bool consumes_touch_events_;

  DISALLOW_COPY_AND_ASSIGN(TestEventFilterWindowDelegate);
};

Window* CreateWindow(int id, Window* parent, WindowDelegate* delegate) {
  Window* window = new Window(
      delegate ? delegate :
      test::TestWindowDelegate::CreateSelfDestroyingDelegate());
  window->set_id(id);
  window->Init(ui::LAYER_TEXTURED);
  window->SetParent(parent);
  window->SetBounds(gfx::Rect(0, 0, 100, 100));
  window->Show();
  return window;
}

// Creates this hierarchy:
//
// RootWindow (EF)
//  +- w1 (EF)
//    +- w11
//      +- w111 (EF)
//        +- w1111 <-- target window
TEST_F(EventFilterTest, PreHandle) {
  scoped_ptr<Window> w1(CreateWindow(1, root_window(), NULL));
  scoped_ptr<Window> w11(CreateWindow(11, w1.get(), NULL));
  scoped_ptr<Window> w111(CreateWindow(111, w11.get(), NULL));
  TestEventFilterWindowDelegate d1111;
  scoped_ptr<Window> w1111(CreateWindow(1111, w111.get(), &d1111));

  test::TestEventFilter* root_window_filter = new test::TestEventFilter;
  test::TestEventFilter* w1_filter = new test::TestEventFilter;
  test::TestEventFilter* w111_filter = new test::TestEventFilter;
  root_window()->SetEventFilter(root_window_filter);
  w1->SetEventFilter(w1_filter);
  w111->SetEventFilter(w111_filter);

  w1111->GetFocusManager()->SetFocusedWindow(w1111.get(), NULL);

  // To start with, no one is going to consume any events. All three filters
  // and the w1111's delegate should receive the event.
  test::EventGenerator generator(root_window(), w1111.get());
  generator.PressLeftButton();
  ui::KeyEvent key_event(ui::ET_KEY_PRESSED, ui::VKEY_A, 0);
  root_window()->AsRootWindowHostDelegate()->OnHostKeyEvent(&key_event);

  // TODO(sadrul): TouchEvent!
  EXPECT_EQ(1, root_window_filter->key_event_count());
  EXPECT_EQ(1, w1_filter->key_event_count());
  EXPECT_EQ(1, w111_filter->key_event_count());
  EXPECT_EQ(1, d1111.key_event_count());
  EXPECT_EQ(1, root_window_filter->mouse_event_count());
  EXPECT_EQ(1, w1_filter->mouse_event_count());
  EXPECT_EQ(1, w111_filter->mouse_event_count());
  EXPECT_EQ(1, d1111.mouse_event_count());
  EXPECT_EQ(0, root_window_filter->touch_event_count());
  EXPECT_EQ(0, w1_filter->touch_event_count());
  EXPECT_EQ(0, w111_filter->touch_event_count());
  EXPECT_EQ(0, d1111.touch_event_count());

  d1111.ResetCounts();
  root_window_filter->ResetCounts();
  w1_filter->ResetCounts();
  w111_filter->ResetCounts();

  // Now make w1's EF consume the event.
  w1_filter->set_key_event_handling_result(ui::ER_CONSUMED);
  w1_filter->set_mouse_event_handling_result(ui::ER_CONSUMED);

  generator.ReleaseLeftButton();
  root_window()->AsRootWindowHostDelegate()->OnHostKeyEvent(&key_event);

  // TODO(sadrul): TouchEvent/GestureEvent!
  EXPECT_EQ(1, root_window_filter->key_event_count());
  EXPECT_EQ(1, w1_filter->key_event_count());
  EXPECT_EQ(0, w111_filter->key_event_count());
  EXPECT_EQ(0, d1111.key_event_count());
  EXPECT_EQ(1, root_window_filter->mouse_event_count());
  EXPECT_EQ(1, w1_filter->mouse_event_count());
  EXPECT_EQ(0, w111_filter->mouse_event_count());
  EXPECT_EQ(0, d1111.mouse_event_count());
  EXPECT_EQ(0, root_window_filter->touch_event_count());
  EXPECT_EQ(0, w1_filter->touch_event_count());
  EXPECT_EQ(0, w111_filter->touch_event_count());
  EXPECT_EQ(0, d1111.touch_event_count());
}

// Tests PostHandle* methods for this hierarchy:
// RootWindow
//  w1 (EF)
//   w11 <-- target window
TEST_F(EventFilterTest, PostHandle) {
  scoped_ptr<Window> w1(CreateWindow(1, root_window(), NULL));
  TestEventFilterWindowDelegate d11;
  scoped_ptr<Window> w11(CreateWindow(11, w1.get(), &d11));

  test::TestEventFilter root_window_filter;
  test::TestEventFilter* w1_filter = new test::TestEventFilter;
  root_window()->AddPostTargetHandler(&root_window_filter);
  w1->AddPostTargetHandler(w1_filter);

  w1->GetFocusManager()->SetFocusedWindow(w11.get(), NULL);

  // TODO(sadrul): TouchEvent/GestureEvent!

  // To start with, no one is going to consume any events. The post-
  // event filters and w11's delegate will be notified.
  test::EventGenerator generator(root_window(), w11.get());

  d11.set_key_event_handling_result(ui::ER_UNHANDLED);
  d11.set_mouse_event_handling_result(ui::ER_UNHANDLED);
  d11.set_consumes_touch_events(false);

  generator.PressKey(ui::VKEY_A, 0);
  generator.PressLeftButton();

  EXPECT_EQ(1, w1_filter->key_event_count());
  EXPECT_EQ(1, w1_filter->mouse_event_count());
  EXPECT_EQ(1, root_window_filter.mouse_event_count());
  EXPECT_EQ(1, d11.key_event_count());
  EXPECT_EQ(1, d11.mouse_event_count());

  root_window_filter.ResetCounts();
  w1_filter->ResetCounts();
  d11.ResetCounts();
  generator.set_flags(0);

  // Let |w1_filter| handle (but not consume) an event. The root-window's
  // post-target filter should still receive the event.
  w1_filter->set_mouse_event_handling_result(ui::ER_HANDLED);
  generator.PressLeftButton();
  EXPECT_EQ(1, d11.mouse_event_count());
  EXPECT_EQ(1, w1_filter->mouse_event_count());
  EXPECT_EQ(1, root_window_filter.mouse_event_count());

  root_window_filter.ResetCounts();
  w1_filter->ResetCounts();
  d11.ResetCounts();
  generator.set_flags(0);

  // Let |w1_filter| consume an event. So the root-window's post-target
  // filter should no longer receive the event.
  w1_filter->set_mouse_event_handling_result(ui::ER_CONSUMED);
  generator.PressLeftButton();
  EXPECT_EQ(1, d11.mouse_event_count());
  EXPECT_EQ(1, w1_filter->mouse_event_count());
  EXPECT_EQ(0, root_window_filter.mouse_event_count());

  // Now we'll have the delegate handle the events.
  root_window_filter.ResetCounts();
  w1_filter->ResetCounts();
  d11.ResetCounts();
  generator.set_flags(0);

  w1_filter->set_mouse_event_handling_result(ui::ER_UNHANDLED);
  d11.set_key_event_handling_result(ui::ER_HANDLED);
  d11.set_mouse_event_handling_result(ui::ER_HANDLED);
  d11.set_consumes_touch_events(true);

  generator.PressKey(ui::VKEY_A, 0);
  generator.PressLeftButton();

  EXPECT_EQ(1, d11.key_event_count());
  EXPECT_EQ(1, d11.mouse_event_count());
  // The delegate processed the event. But it should still bubble up to the
  // post-target filters.
  EXPECT_EQ(1, w1_filter->key_event_count());
  EXPECT_EQ(1, root_window_filter.key_event_count());
  EXPECT_EQ(1, w1_filter->mouse_event_count());
  EXPECT_EQ(1, root_window_filter.mouse_event_count());

  // Now we'll have the delegate consume the events.
  root_window_filter.ResetCounts();
  w1_filter->ResetCounts();
  d11.ResetCounts();
  generator.set_flags(0);

  d11.set_key_event_handling_result(ui::ER_CONSUMED);
  d11.set_mouse_event_handling_result(ui::ER_CONSUMED);
  d11.set_consumes_touch_events(true);

  generator.PressKey(ui::VKEY_A, 0);
  generator.PressLeftButton();

  EXPECT_EQ(1, d11.key_event_count());
  EXPECT_EQ(1, d11.mouse_event_count());
  // The delegate consumed the event. So it should no longer reach the
  // post-target filters.
  EXPECT_EQ(0, w1_filter->key_event_count());
  EXPECT_EQ(0, root_window_filter.key_event_count());
  EXPECT_EQ(0, w1_filter->mouse_event_count());
  EXPECT_EQ(0, root_window_filter.mouse_event_count());

  // Now we'll have the pre-filter methods consume the events.
  w1->RemovePostTargetHandler(w1_filter);
  w1->SetEventFilter(w1_filter);
  w1_filter->ResetCounts();
  d11.ResetCounts();
  generator.set_flags(0);

  d11.set_key_event_handling_result(ui::ER_UNHANDLED);
  d11.set_mouse_event_handling_result(ui::ER_UNHANDLED);
  d11.set_consumes_touch_events(false);

  w1_filter->set_key_event_handling_result(ui::ER_CONSUMED);
  w1_filter->set_mouse_event_handling_result(ui::ER_CONSUMED);
  w1_filter->set_consumes_touch_events(true);

  generator.PressKey(ui::VKEY_A, 0);
  generator.PressLeftButton();

  EXPECT_EQ(1, w1_filter->key_event_count());
  EXPECT_EQ(0, d11.key_event_count());
  EXPECT_EQ(1, w1_filter->mouse_event_count());
  EXPECT_EQ(0, d11.mouse_event_count());
}

}  // namespace aura
